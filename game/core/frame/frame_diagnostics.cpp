#include "frame/frame_diagnostics.h"

#include "audio/libsnd_globals.h" // kSeqTickFn / the tick descriptor's slots
#include "cfg.h"
#include "core.h"
#include "engine/engine.h"
#include "entry/game_ctx.h"

#include <lucent/log.h>

namespace tomba {
namespace {

constexpr uint32_t kTaskBase = 0x801fe000u;
constexpr uint32_t kTaskStride = 0x70u;
constexpr uint32_t kTaskEntry = 0x0cu;

const char *stageName(const GameConfig &cfg, uint32_t entry) {
  if (entry == cfg.stageStart) {
    return "START";
  }
  if (entry == cfg.stageDemo) {
    return "DEMO";
  }
  if (entry == cfg.stageGame) {
    return "GAME";
  }
  return "?";
}

} // namespace

void FrameDiagnostics::afterFrame(Core &core, uint32_t frame) {
  const GameConfig &cfg = *core.cfg;
  const uint32_t seqState = (core.mem_r16(0x801054b0u) << 16) | (core.mem_r32(0x80104c28u) & 0xffffu);
  if (seqState != seqLast_) {
    lucent::debug("seq",
                  "[seqdbg] f{} open={} playmask=0x{:04X} tickmode={} seqfn=0x{:08X} stage=0x{:08X}",
                  frame,
                  core.mem_r16s(0x801054b0u),
                  core.mem_r32(0x80104c28u) & 0xffffu,
                  core.mem_r8(tomba::audio::libsnd::kTickMode),
                  core.mem_r32(tomba::audio::libsnd::kSeqTickFn),
                  core.mem_r32(kTaskBase + kTaskEntry));
    seqLast_ = seqState;
  }

  lucent::debug("cam",
                "f{} ({},{},{})",
                frame,
                core.mem_r16s(0x1f8000d2u),
                core.mem_r16s(0x1f8000d6u),
                core.mem_r16s(0x1f8000dau));

  static const lucent::Channel stateChannel{"state"};
  if (stateChannel) {
    uint64_t signature = 0;
    int menuSlot = -1;
    uint8_t menuPage = 0;
    for (int slot = 0; slot < 3; ++slot) {
      const uint32_t base = kTaskBase + static_cast<uint32_t>(slot) * kTaskStride;
      const uint16_t state = core.mem_r16(base);
      const uint32_t entry = core.mem_r32(base + kTaskEntry);
      signature = signature * 1099511628211ull + (static_cast<uint64_t>(state) << 32 | entry);
      if (state && (entry & 0xfffff000u) == 0x80108000u) {
        menuSlot = slot;
        menuPage = core.mem_r8(base + 0x6bu);
      }
    }
    signature = signature * 31u + (static_cast<uint64_t>(menuSlot) << 8 | menuPage);
    if (signature != stateLastSignature_) {
      stateLastSignature_ = signature;
      lucent::Line line;
      line.add("f{}", frame);
      for (int slot = 0; slot < 3; ++slot) {
        const uint32_t base = kTaskBase + static_cast<uint32_t>(slot) * kTaskStride;
        line.add(" | s{} st={} ent=0x{:08X}", slot, core.mem_r16(base), core.mem_r32(base + kTaskEntry));
      }
      line.add("  MENU={}", menuSlot >= 0 ? "OPEN" : "no");
      if (menuSlot >= 0) {
        line.add("(slot{} page={})", menuSlot, menuPage);
      }
      line.add("  cam=({},{},{}) sm[0x4a]={}",
               core.mem_r16s(0x1f8000d2u),
               core.mem_r16s(0x1f8000d6u),
               core.mem_r16s(0x1f8000dau),
               core.mem_r16(kTaskBase + 0x4au));
      line.flush_debug(stateChannel);
    }
  }

  if (cfg_str("PSXPORT_BGMDBG")) {
    for (int slot = 0; slot < 14; ++slot) {
      const uint32_t sequence = 0x800be3d8u + static_cast<uint32_t>(slot) * 0xb0u;
      const uint32_t flags = core.mem_r32(sequence + 0x98u);
      const uint32_t readPointer = core.mem_r32(sequence);
      if ((flags & 1u) && readPointer != bgmReadPointers_[slot]) {
        lucent::info("bgmtick",
                     "f{} slot{} active rdptr={:08X} base={:08X} ({:+})",
                     frame,
                     slot,
                     readPointer,
                     core.mem_r32(sequence + 4u),
                     static_cast<int>(readPointer - core.mem_r32(sequence + 4u)));
        bgmReadPointers_[slot] = readPointer;
      }
      if (!(flags & 1u)) {
        bgmReadPointers_[slot] = 0;
      }
    }
  }

  const uint32_t stageEntry = core.mem_r32(kTaskBase + kTaskEntry);
  const uint32_t state48 = core.mem_r16(kTaskBase + 0x48u);
  const uint32_t outerState =
      (state48 << 24) | (core.mem_r16(kTaskBase + 0x4au) << 16) | (core.mem_r16(kTaskBase + 0x4cu) << 8);
  const uint32_t leafState = core.mem_r16(kTaskBase + 0x4eu) ^ (core.mem_r16(kTaskBase + 0x50u) << 12) ^
                             (core.mem_r16(kTaskBase + 0x52u) << 4);
  const uint32_t stateMachine = outerState | leafState;
  if (stageEntry != lastStageEntry_ || stateMachine != lastStateMachine_) {
    lucent::info("tomba-frame",
                 "  frame {}: stage={}(0x{:08X}) sm[48={} 4a={} 4c={} 4e={} 50={} 52={}] "
                 "@0x80109450={:08X}",
                 frame,
                 stageName(cfg, stageEntry),
                 stageEntry,
                 state48,
                 core.mem_r16(kTaskBase + 0x4au),
                 core.mem_r16(kTaskBase + 0x4cu),
                 core.mem_r16(kTaskBase + 0x4eu),
                 core.mem_r16(kTaskBase + 0x50u),
                 core.mem_r16(kTaskBase + 0x52u),
                 core.mem_r32(0x80109450u));
    lastStageEntry_ = stageEntry;
    lastStateMachine_ = stateMachine;
  }

  if (stageEntry == cfg.stageGame && frame == 75) {
    lucent::debug("stream",
                  "[streamdbg] task2 obj @0x801fe0e0 state={} entry=0x{:08X}",
                  core.mem_r16(0x801fe0e0u),
                  core.mem_r32(0x801fe0ecu));
    lucent::debug("stream",
                  "[streamdbg] startLBA(+54/801fe134)={} endLBA(+58/801fe138)={} "
                  "chan(801fe146)={} be0e4=0x{:02X}",
                  core.mem_r32(0x801fe134u),
                  core.mem_r32(0x801fe138u),
                  core.mem_r8(0x801fe146u),
                  core.mem_r8(0x800be0e4u));
    lucent::debug("stream",
                  "[streamdbg] dest(_DAT_1f8001f8)=0x{:08X} words(_DAT_1f8001f4)={} "
                  "f0={} f1f800224=0x{:08X}",
                  core.mem_r32(0x1f8001f8u),
                  core.mem_r32(0x1f8001f4u),
                  core.mem_r32(0x1f8001f0u),
                  core.mem_r32(0x1f800224u));
  }

  lucent::debug("schedf",
                "f{} t0[st={} e={:08X} s48={} s4a={} s4c={} s5c={}] t1[st={}] t2[st={}]",
                frame,
                core.mem_r16(kTaskBase),
                stageEntry,
                state48,
                core.mem_r16(kTaskBase + 0x4au),
                core.mem_r16(kTaskBase + 0x4cu),
                core.mem_r16(kTaskBase + 0x5cu),
                core.mem_r16(kTaskBase + kTaskStride),
                core.mem_r16(kTaskBase + 2u * kTaskStride));
  if (frame < 10 || (frame % 30u) == 0) {
    lucent::info("tomba-frame",
                 "  frame {}: t0[st={} e=0x{:08X} s48={}] t1[st={}] t2[st={}] f135={}",
                 frame,
                 core.mem_r16(kTaskBase),
                 stageEntry,
                 state48,
                 core.mem_r16(kTaskBase + kTaskStride),
                 core.mem_r16(kTaskBase + 2u * kTaskStride),
                 core.mem_r8(0x1f800135u));
  }

  // The frame-rate decision and the guest's own answer to it, so a reader sees the cadence rather
  // than inferring it. `spent/quota` is what the port spent this frame against the decision;
  // `dwell` is the GUEST's counter, which its own vsync callback advances and which decides the
  // gate; `unaccounted` must be 0, because a non-zero value means the logic frame claimed display
  // time no work was scheduled on. Same 30-frame stride as the line above, so it costs nothing
  // when nobody asked for it.
  if (frame < 10 || (frame % 30u) == 0) {
    const FrameCadence &cadence = eng(&core).cadence();
    lucent::info("tomba-frame",
                 "  cadence f{}: spent {}/{} fields, guest dwell counter {} -> gate {}, "
                 "frames={} fields spent total={} unaccounted={}",
                 frame,
                 cadence.vblanksThisLogicFrame(),
                 cadence.vblanksPerLogicFrame(),
                 cadence.dwellCounter(),
                 cadence.gateOpen() ? "OPEN" : "SHUT",
                 cadence.logicFrames(),
                 cadence.vblanksSpent(),
                 cadence.unaccountedVblanks());
  }
}

} // namespace tomba
