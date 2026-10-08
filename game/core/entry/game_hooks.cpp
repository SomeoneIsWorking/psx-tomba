// game_hooks.cpp — bounded compatibility callbacks not yet migrated into TombaRuntime.
//
// Each remaining hook reaches one game subsystem from a generic framework consumer. The runtime
// directly owns context lifecycle, boot, and override registration; those slots stay null so the
// callback bag cannot become a second authority.
#include "core.h"
#include "debug/dev_warp.h"
#include "engine/engine.h"
#include "entry/game_ctx.h"
#include "game.h"
#include "game_iface.h"
#include "title_facts.h"
#include <string.h>

// REPL camera diagnostics — reach the game's engine subsystem without exposing its type to psxport.
static void tomba_replCamTeleport(Core *c, int x, int y, int z) {
  eng(c).camTeleport(x, y, z);
}
static void tomba_replCamTeleportOff(Core *c) {
  eng(c).camTeleportOff();
}
// One complete cold warp shared by the standalone REPL and the SBS oracle. The framework used to
// split this operation across generic code and two hooks, duplicating Tomba's state-machine layout;
// SBS then combined the destination preload with an old-area door transition and ran stale objects
// against the new table. All game addresses and ordering live here now.
static void tomba_devWarp(Core *c, int area, int sub) {
  tomba::applyColdWarp(*c, area, sub);
}
// dev-warp area index (game/core/dev_areas.cpp) — count / sourced name / "is a
// warp legal now".
static int tomba_devAreaCount(Core *) {
  return Engine::devAreaCount();
}
static const char *tomba_devAreaName(Core *, int area) {
  return Engine::devAreaName(area);
}
static bool tomba_devWarpAllowed(Core *c) {
  return Engine::devWarpAllowed(c);
}
// game-side REPL commands (invtest/bgm/bgmstop/seqsolo/musictest) — body in
// game/core/repl_commands.cpp.
extern bool tomba_repl_command(Core *c, const char *cmd, const char *line);

static void tomba_frameUpdate(Core *c) {
  eng(c).frameUpdate();
}
static void tomba_drawOTag(Core *c, uint32_t otHead) {
  eng(c).drawOTag(otHead);
}
static void tomba_musicCoordTick(Core *c) {
  eng(c).musicCoord.tick();
}
static bool tomba_cdDialogToneActive(Core *c) {
  return eng(c).musicCoord.dialogToneActive();
}
static void tomba_cdMusicFadeIn(Core *c) {
  eng(c).musicCoord.musicFadeIn();
}

// tomba_audioMixFrame — mix the game's native music engine on top of the SPU's
// drained PCM. Was the direct native_music mix in spu_audio.cpp::frameEx:
// render into a per-frame scratch, saturating-add. `frames` is the SPU sink's
// per-video-frame count, capped at SPU_FRAMES_PER_VIDEO_FRAME(735)+64.
static void tomba_audioMixFrame(Core *c, int16_t *buf, int frames) {
  NativeMusic &nm = gctx(c)->native_music;
  if (!nm.active()) {
    return;
  }
  const int kMaxFrames = 735 + 64; // mirrors the SPU host-sink per-frame cap (spu_audio.cpp)
  if (frames > kMaxFrames) {
    frames = kMaxFrames;
  }
  int16_t mbuf[2 * (735 + 64)]; // one video frame of interleaved stereo scratch
  nm.render(mbuf, frames);
  for (int i = 0; i < frames * 2; i++) {
    int v = buf[i] + mbuf[i];
    if (v > 32767) {
      v = 32767;
    } else if (v < -32768) {
      v = -32768;
    }
    buf[i] = (int16_t)v;
  }
}
// Sound-Test / HUD music readout — reach the game's MusicList (was
// rmlui_overlay's direct game->music_list.*).
static const char *tomba_audioNowPlayingName(Core *c) {
  int np = gctx(c)->music_list.nowPlaying();
  return (np >= 0) ? gctx(c)->music_list.name(np) : nullptr;
}
static void tomba_audioSoundTestPlay(Core *c, int track) {
  if (track < 0) {
    gctx(c)->music_list.stop();
  } else {
    gctx(c)->music_list.play(track);
  }
}

// tomba_schedFreshEntry — the fresh-task-entry native stage-body dispatch,
// moved out of the generic scheduler stanza. entryPc is
// the fresh resume_pc. Two native stages:
//   * GAME stagePrologue (cfg->stageGame == 0x8010637C): set the guest-continuation
//   target and run stageMain,
//     which runs stagePrologue + requestGuestContinuation(0x801063F4) → leaves
//     c->guest_continuation_pc set. Return FALSE: the caller continues to
//     guest executor, taking the redirect start from c->guest_continuation_pc.
//   * STAGE-0 startBinStage (cfg->stageStart == 0x8010649C): run the terminal
//   startBinStage body. Return
//     TRUE: the caller finalizes the stage-0 slot (task_ctx/base=2/in_stage=0)
//     and early-returns the tick WITHOUT running guest executor.
// A non-stage fresh entry matches neither: returns false with
// c->guest_continuation_pc untouched (0), so the caller runs guest executor at the
// plain resume_pc — exactly the original else-fall-through.
static bool tomba_schedFreshEntry(Core *c, int /*slot*/, uint32_t /*base*/, uint32_t entryPc) {
  if (entryPc == c->cfg->stageGame) {
    eng(c).stageMain(); // stagePrologue + redirect to 0x801063F4
    return false;       // continue to guest executor with the redirect start
  }
  if (entryPc == c->cfg->stageStart) {
    eng(c).startBinStage(); // STAGE-0 fresh; terminal — skip guest executor
    return true;
  }
  return false; // not a native stage: plain guest executor at resume_pc
}

static bool tomba_hasNativeHandlerForEntry(Core *c, uint32_t entryPc) {
  return c->game->pcSched.hasNativeHandlerForEntry(entryPc);
}

// tomba_schedStageBody — run the SchedBody-selected game stage body.
// PcScheduler (framework) owns the task/coro/yield machinery and calls this for
// the actual Engine::* stage body, so the framework names no Engine method.
// Returns the body's int result (Engine::frame's `handled`; 0 for the void
// bodies).
static int tomba_schedStageBody(Core *c, int which, void *arg) {
  switch (which) {
  case SCHED_DEMO_STAGEMAIN:
    eng(c).demo.stageMain();
    return 0;
  case SCHED_DEMO_FRAME:
    eng(c).demo.frame();
    return 0;
  case SCHED_GAME_PROLOGUE:
    eng(c).stagePrologue();
    return 0;
  case SCHED_GAME_FRAME:
    return eng(c).frame();
  case SCHED_SOP_AREALOAD:
    eng(c).sop.areaLoad();
    return 0;
  case SCHED_CORO_TEXGROUP:
    eng(c).asset.loadTexgroup();
    return 0;
  case SCHED_CORO_PRELOAD1:
    eng(c).asset.preloadStage1AsTask();
    return 0;
  case SCHED_CORO_AREADATA:
    eng(c).asset.areaDataLoadAsTask();
    return 0;
  case SCHED_CORO_AREALOAD_FAITHFUL:
    eng(c).sop.areaLoadFaithful();
    return 0;
  case SCHED_FIBER_STARTBIN:
    eng(c).startBinStageFaithful();
    return 0;
  case SCHED_FIBER_DEMO_BODY:
    eng(c).demo.stageBodyFaithful();
    return 0;
  case SCHED_FIBER_STAGE_BODY:
    eng(c).stageBodyFaithful();
    return 0;
  default:
    return 0;
  }
}
static uint32_t tomba_schedRng(Core *c) {
  return rngOf(c).next();
} // FUN_8009A450 (guest seed 0x80105EE8)

// Designated initializers make this the exact inventory of compatibility callbacks. The direct
// GameRuntime slots (ctxCreate/ctxDestroy/bootInit/registerOverrides) are intentionally absent.
static const GameHooks g_tomba_hooks = {
    .frameUpdate = tomba_frameUpdate,
    .drawOTag = tomba_drawOTag,
    .musicCoordTick = tomba_musicCoordTick,
    .cdDialogToneActive = tomba_cdDialogToneActive,
    .cdMusicFadeIn = tomba_cdMusicFadeIn,
    .audioMixFrame = tomba_audioMixFrame,
    .audioNowPlayingName = tomba_audioNowPlayingName,
    .audioSoundTestPlay = tomba_audioSoundTestPlay,
    .schedFreshEntry = tomba_schedFreshEntry,
    .hasNativeHandlerForEntry = tomba_hasNativeHandlerForEntry,
    .replCamTeleport = tomba_replCamTeleport,
    .replCamTeleportOff = tomba_replCamTeleportOff,
    .replCommand = tomba_repl_command,
    .devWarp = tomba_devWarp,
    .devAreaCount = tomba_devAreaCount,
    .devAreaName = tomba_devAreaName,
    .devWarpAllowed = tomba_devWarpAllowed,
    .schedStageBody = tomba_schedStageBody,
    .schedRng = tomba_schedRng,
};

const GameHooks &tomba::title::hooks() {
  return g_tomba_hooks;
}
