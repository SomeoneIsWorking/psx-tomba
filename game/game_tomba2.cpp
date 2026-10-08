// Tomba!2-specific native overrides (per-game tier). Generic mechanisms live in timing.c /
// cd_override.c; this file holds glue tied to MAIN.EXE's own addresses.
//
// VBlank pacing: the port owns the engine's own frame-rate lever (don't dwell)
// ------------------------------------------------------------------------
// The StrPlayer main loop FUN_80050b08 paces each displayed frame with a busy-wait inside its
// loop body, at 0x80050CC8..0x80050CF4:
//     DAT_800e809c = 0;  FUN_800788AC();  FUN_80051e60();  DrawSync(0);
//     do {} while (DAT_800e809c < DAT_1f800235);
// On hardware the VBlank IRQ bumps DAT_800e809c (0x800E809C, u16) until it reaches the per-frame
// quota DAT_1f800235 (scratchpad u8, =2 => the engine's 30 fps logic rate). A census of the
// authenticated resident image finds exactly two references to that byte in the whole text: the
// one store, whose literal is compiled into the instruction word `li v0,0x2`, and the gate's one
// load. The guest therefore has no runtime 30/60 switch at all — the decision is the port's.
//
// `FrameCadence` (game/core/frame_cadence.h) holds that decision, publishes it into the guest's
// own quota byte through the same single store FUN_80050a0c makes, and advances the guest's
// dwell counter once per consumed display field. THIS PRODUCT NEVER EXECUTES THE GATE: psxport
// calls GameRuntime::bootInit, which runs FUN_80050b08's init prefix and then hands iteration to
// TombaFrameDriver, and 0x80050CC8 is a label inside that body rather than a function entry, so
// it is not a legal override key in the first place. What the port can own is the gate's state,
// and that is what FrameCadence owns. Nothing here simulates an interrupt: the per-field work
// (libsnd sequencer tick + the SPU's 1/60 s advance) runs once per field this logic frame spans,
// which is the same work and the same count the vsync callback's slot-4 callback would have
// triggered. The previous code here ASSIGNED the quota to the dwell counter to "satisfy the
// dwell"; with the gate not executing, and the counter's only other reader being the vsync
// callback the port never raises, that store was unreachable by anything — the counter is now
// counted, which ends each logic frame at the same value for a stated reason.

#include "animation.h"            // PC-native per-object animation-VM subsystem
#include "audio/libsnd_globals.h" // kSeqTickFn — the tick slot the wrapper below dispatches
#include "cfg.h"
#include "collision.h" // PC-native collision-grid subsystem
#include "core.h"
#include "core/assets/asset.h"  // PC-native asset-loading subsystem (extracted from this file)
#include "core/assets/str.h"    // Resident string leaves
#include "core/engine/engine.h" // class Engine — this file defines Engine::frameUpdate / Engine::drawOTag
#include "core/entry/game_ctx.h"
#include "core/overrides/native_override_catalog.h"
#include "cull.h" // PC-native visibility cull / LOD subsystem
#include "game.h"
#include "guest_call.h"
#include "mathlib.h"         // PC-native math/PRNG leaf primitives (rand, trig LUTs, bit-test)
#include "mods.h"            // g_mods (fps60 persisted with the other user settings)
#include "ui/font.h"         // Font::registerOverrides — drawText/glyphEmit
#include "ui/options_page.h" // OptionsPage::install — the OPTIONS page backdrop (FUN_8007FC24)
#include "ui/panel.h"        // Panel::install — FUN_8004FFB4 panel fill quad
#include <stdio.h>
#include <stdlib.h>
// g_render_object retired 2026-07-03 — was defined + written by Cull::objectCull but never read; dead.
// g_fps60_on retired — read g_mods.fps60 (mods.h)
// SpuAudio methods are called via c->game->spu_audio (owner: Game, see runtime/psx/spu_audio.h).

// Both guest frame-rate fields are owned by tomba::FrameCadence (game/core/frame_cadence.h):
// kQuotaAddress 0x1F800235 (u8, the gate's threshold) and kDwellCounterAddress 0x800E809C
// (u16, the dwell counter its vsync callback increments). There are deliberately no address
// macros for them here: a second copy of a measured address is how the two started to drift.

// libsnd music-sequencer tick (RE: docs/journal.md later-53; SsSetTickMode = FUN_80090750).
// Tomba2 sequences its in-game/menu BGM with the libsnd sequencer, ticked from the VBlank IRQ
// (tick mode 5, RCnt3/vblank). The IRQ runs the tick wrapper FUN_800909c0, which chains the
// optional per-vblank user callback (DAT_800ac430) then the sequencer SsSeqCalled (DAT_800ac42c).
// The port delivers NO preemptive IRQ and collapses the pace-dwell the IRQ would fire in, so on
// hardware-faithful boot the sequencer never ticks -> zero per-note KON -> silent SPU (verified
// vs the oracle, later-53: the oracle writes KON from this very ISR while parked in the dwell).
// FIX (port the HW interrupt work to PC, per the busy-wait-porting rule — NOT simulate the IRQ):
// run the same tick wrapper natively once per vblank. The wrapper/sequencer are NOT emitted by
// the native overrides (only reached via the IRQ callback pointer, never a direct jal), so we
// invoke them through the PSXPort guest boundary; it runs
// FUN_800909c0 to its `jr ra` and returns. Caller-saved regs it clobbers are dead across the
// FUN_800788ac call site by MIPS convention, so this is safe to run right after the super-call.
#define SEQ_TICK_WRAPPER 0x800909C0u // FUN_800909c0: per-vblank libsnd tick (user cb + SsSeqCalled)
// The slot the wrapper dispatches UNCONDITIONALLY, read from the one owner (libsnd_globals.h) and
// not spelled here: a second copy of a measured address is how the boot guard and the tick drifted
// onto different pages in the first place (issue 0026), and this guard only means anything while it
// tests the SAME word the dispatch reads.

#include "audio/libsnd_globals.h" // kSeqTickFn — the tick slot the wrapper below dispatches
#include "gpu_perf.h"             // per-frame CPU phase profiler (REPL `debug perf`), default off

// Per-frame engine tick. Called DIRECTLY (a plain C call) from TombaFrameDriver —
// this is the PC-driven game loop's update/audio body. It runs the still-PSX per-frame update leaf and
// per-vblank audio, then returns. TombaFrameDriver owns the one presentation fence and the rest of the
// measured frame transaction. NOT an override anymore; not static so the driver can call it top-down.
void Engine::frameUpdate() {
  Core *c = this->core;
  devTeleportApply(); // dev `tp X Y Z`, before the frame's state update. HERE and not in a stage/
                      // camera body because this is the ONE per-frame body every exec path runs
                      // (TombaFrameDriver calls it directly), so the teleport no longer depends on
                      // which camera mode, stage or exec leg the run happens to be in — see
                      // engine.h's mCamTpPending banner for what that dependency measured.
  // Phase::PadFence brackets exactly the one call below. It was once named LOGIC and described as
  // "all guest interpreter work + render submit", which stopped being true when 0x800788AC became
  // natively owned (Engine::padEdgeFence) and the per-frame game work moved to PcScheduler, which
  // Phase::GameLogic brackets. A ~0.00 ms phase called LOGIC reads as "the game is free".
  c->game->perf.phaseBegin(GpuPerf::Phase::PadFence);
  psx::cpu::dispatchGuestToReturn0(*c,
                                   0x800788ACu,
                                   psx::cpu::ExecutionBudget::currentTurn(*c),
                                   __func__); // real per-frame state update (still-PSX leaf)
  c->game->perf.phaseEnd(GpuPerf::Phase::PadFence);
  // Per-VBLANK audio work. On hardware the libsnd sequencer ticks once per VBlank IRQ (60 Hz NTSC)
  // and the SPU plays in realtime. One logic frame is however many display fields the engine's own
  // frame-rate decision says it spans — DAT_1F800235, the byte FUN_80050b08's gate compares its
  // vblank-ticked dwell counter against. The PORT owns that decision now (FrameCadence,
  // game/core/frame_cadence.h) rather than re-reading a literal back out of guest memory the port
  // itself wrote; the decision is published into that same guest byte, so this loop and anything
  // reading the field are driven by one number. Retail is 2 fields per logic frame, Tomba! 2's
  // 30 fps logic rate, so the per-vblank work runs twice per logic frame and stays at the
  // hardware 60 Hz rate in real time. later-54 ran BOTH once (matching each other but at half
  // real-time); windowed that plays audio at HALF tempo — the user heard the menu-cursor tick too
  // slow (the headless WAV hid it: its timeline is field-count, not wall-clock, so 1 tick/1 field
  // there is still 60:60 = correct-sounding). Running both quota× fixes real-time playback and
  // keeps the WAV's tick:field ratio unchanged (just a longer, more correct duration).
  // Sequencer guard: pointer initialized + sane code address (never call through null pre-SsStart).
  // A decision of 1 field per logic frame — the guest's own 60 fps lever — ticks once.
  const int quota = cadence().vblanksPerLogicFrame();
  uint32_t seqfn = c->mem_r32(tomba::audio::libsnd::kSeqTickFn);
  const bool seq_ok = (seqfn & 0x1FFFFFFFu) >= 0x10000u && (seqfn & 0x1FFFFFFFu) < 0x200000u;
  c->game->perf.phaseBegin(GpuPerf::Phase::Audio); // per-vblank sequencer tick + SPU advance
  for (int v = 0; v < quota; v++) {                // once per VBlank this logic frame spans
    // Count the field, and let the GUEST advance its own dwell counter: the sequencer tick below
    // reaches runVblankCallbacks, whose vsync-callback slot 4 is the game's LAB_800506b4, and that
    // body is what performs `DAT_800e809c += 1`. The port does not write the counter — see
    // game/core/frame_cadence.h, which records why a revision that did was wrong.
    cadence().consumeVblank();
    if (seq_ok) {
      psx::cpu::dispatchGuestToReturn0(*c,
                                       SEQ_TICK_WRAPPER,
                                       psx::cpu::ExecutionBudget::currentTurn(*c),
                                       __func__); // libsnd per-vblank tick (user cb + SsSeqCalled)
    }
    c->game->spu_audio.frame(); // advance SPU one 1/60 s field + feed device
  }
  cadence().accountVblankWork();
  // (native field-BGM director REMOVED — it played a HARDCODED song over everything from the menu on.
  //  Music is the guest libsnd path above; no native music engine, no hardcoded song.)
  c->game->perf.phaseEnd(GpuPerf::Phase::Audio);
}

// DrawOTag (libgpu FUN_80081560): the guest's ordering table goes to the GPU device as GP0.
void Engine::drawOTag(uint32_t otHead) { // called directly from TombaFrameDriver; not an override
  Core *c = this->core;
  lucent::debug("drawotag", "f{} drawOTag otHead={:08X}", c->game->gpu.s_frame, otHead);
  gpu_dma2_linked_list(c, otHead);
  c->game->rq.flush(c);
  captureBugReportReference(otHead);
}

// A B-key bug report is taking this frame: put the PSX render of the same ordering table beside the
// native picture (psx::debug::BugReportSession). Read-only over the frame just drawn.
void Engine::captureBugReportReference(uint32_t otHead) {
  if (core->game->bugReport.capturingFrame()) {
    core->game->bugReport.captureReferenceFrame(*core, otHead);
  }
}

void games_tomba2_init(void) {
  // ONE behavior = the PC game. The native engine path is registered UNCONDITIONALLY — no FAITHFUL master
  // switch, no per-override *_RECOMP / NO_* opt-outs (those were faithful-first A/B scaffolding; the user
  // directive is no gating + drive toward removing the interpreter entirely, so they are retired). Every
  // override below IS the behavior; the user verifies it via ./run.sh.
  // Hand-written native C++ for the boot→first-cutscene path (game_tomba2.cpp).
  // (games_native_path_init removed: native_misc.cpp was dead reference scaffolding — later-288)
  // OVERRIDE SYSTEM REMOVED (2026-06-22): the whole `_register()` scaffolding block used to install
  // tomba::native::declareOverride() entries. The override table is gone; the per-subsystem register functions
  // (engine_math_register, save_register, sound_register, hud_register, actor_sm_24448_register, and
  // the beh_*_register siblings) were left as empty stubs "in case." Every stub had a zero- or single-
  // dead-line body — dead scaffolding — and got deleted. Direct-call wiring is the shape now.
  void perobj_dispatch_install();
  perobj_dispatch_install(); // FUN_8003CDD8/FUN_8003F698 substrate-mirror ownership (band 0x8003xxxx)
  void perobj_billboard_install();
  perobj_billboard_install(); // FUN_8003CCA4/C2D4/C464/C8F4 substrate-mirror ownership (band 0x8003xxxx)
  void text_label_install();
  text_label_install(); // FUN_80039F4C text-label renderer (Render::textLabelEmit)
  void render_walk_dispatch_install();
  render_walk_dispatch_install(); // FUN_8003C048 render-walk loop ownership (band 0x8003xxxx)
  void overlay_type_dispatch_install();
  overlay_type_dispatch_install(); // FUN_8003D0BC per-area-type overlay dispatch (band 0x8003xxxx)
  void objlist_walk_install();
  objlist_walk_install(); // FUN_8003BB50/BCF4/BED8/BF00/EEC0 object-list walkers (band 0x8003xxxx)
  void gpu_dma_queue_install();
  gpu_dma_queue_install(); // FUN_80082D04/FB4/83364/82424 GPU-DMA completion-queue cluster
  void gpu_libgpu_leaves_install();
  gpu_libgpu_leaves_install(); // FUN_80080F6C/81458 DrawSync/ClearOTagR (libgpu GPU-sys jump table)
  void gpu_loadimage_streamer_install();
  gpu_loadimage_streamer_install(); // FUN_80082734 libgpu LoadImage() chunked GP0-FIFO pixel streamer
  void gpu_putdrawenv_install();
  gpu_putdrawenv_install();        // FUN_800815D0 PutDrawEnv + 4 DRAWENV field-word builders
  Font::registerOverrides();       // FUN_80079374/80078CA8 Font::drawText/glyphEmit (hottest unowned leaves)
  tomba::Str::registerOverrides(); // FUN_80079528 Str::length (generic strlen, hottest unowned leaf)
  Asset::registerOverrides();      // FUN_80045558 indexed load into the AREA slot + code-image publish
  Panel::install();                // FUN_8004FFB4 panel fill quad
  OptionsPage::install();          // FUN_8007FC24 OPTIONS page backdrop packet
  void pad_edge_fence_install();
  pad_edge_fence_install(); // FUN_800788AC per-frame input-edge fence (banked draft, §9-verified)
  cfg_logf("engine", "native object-list walk active (FUN_8007a904)");
}
