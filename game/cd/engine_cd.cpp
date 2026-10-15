// engine_cd.cpp — Tomba! 2's CD chokepoints as resident native overrides.
#include "cd/engine_cd.h"

#include "cd/engine_cd_loader.h"

#include "core.h"
#include "entry/session.h"
#include "game.h"
#include "guest_call.h"
#include "overrides/native_override_catalog.h"
#include "sched/task_slot_layout.h"
#include "xa_state.h"

#include <lucent/log.h>

#include <array>

namespace tomba {

namespace {

constexpr int V0 = 2;
constexpr int A0 = 4;
constexpr int A1 = 5;
constexpr int A2 = 6;
constexpr int A3 = 7;

constexpr std::uint32_t kCdCmdStream = 0x8001CE90u;
constexpr std::uint32_t kCdReadPrimitive = 0x8008C1ECu;
constexpr std::uint32_t kCdFileLoad = 0x8001DB8Cu;
constexpr std::uint32_t kCdAsyncRead = 0x8001D940u;
constexpr std::uint32_t kCdInlineLoad = 0x8001DC40u;
constexpr std::uint32_t kVoicePlay = 0x8001D2A8u;
constexpr std::uint32_t kVoiceStop = 0x8001CF2Cu;
constexpr std::uint32_t kCdToSpuMix = 0x8001CF00u;
// Slot 2's state word: the cutscene's clip wait.
constexpr std::uint32_t kVoiceTaskState = sched::taskSlotBase(2) + sched::kTaskState;

// FUN_800898A0's success path installs these CD event callbacks.
constexpr std::array<std::uint32_t, 4> kCdCallbackSlots{0x800ABFBCu, 0x800ABFC0u, 0x800ABF24u, 0x800ABF28u};
constexpr std::array<std::uint32_t, 4> kCdCallbacks{0x8009996Cu, 0x80089994u, 0x800899BCu, 0u};

EngineCd &cdOf(Core *core) {
  return sessionOf(core).cd;
}

std::uint8_t bcd(int value) {
  return static_cast<std::uint8_t>((value % 10) + ((value / 10) << 4));
}

// FUN_8001CE90(cmd, param, result): the streaming CdControl wrapper. Feeds XA mode/filter/seek/play
// to the native streamer; GetlocL reports the task's target sector as already reached.
void cdCommandStream(Core *c) {
  const std::uint32_t cmd = c->r[A0] & 0xFFu;
  const std::uint32_t param = c->r[A1];
  const std::uint32_t result = c->r[A2];
  const auto paramByte = [c, param](std::uint32_t index) {
    return param ? static_cast<std::uint8_t>(c->mem_r8(param + index)) : std::uint8_t{0};
  };
  lucent::debug("cdcmd",
                "cmd=0x{:02X} param=[{:02X} {:02X} {:02X} {:02X}] ra=0x{:08X}",
                cmd,
                paramByte(0),
                paramByte(1),
                paramByte(2),
                paramByte(3),
                c->r[31]);
  psx::audio::XaStream *xa = &c->game->xa;
  switch (cmd) {
  case 0x0E: // Setmode
    xa->setMode(paramByte(0));
    c->game->cdc.setMode(paramByte(0));
    break;
  case 0x0D: // Setfilter
    xa->setFilter(paramByte(0), paramByte(1));
    c->game->cdc.setFilter(paramByte(0), paramByte(1));
    break;
  case 0x02: // Setloc
    if (param) {
      xa->setLoc(paramByte(0), paramByte(1), paramByte(2));
    }
    break;
  case 0x06: // ReadN
  case 0x1B: // ReadS
    xa->start();
    break;
  case 0x08: // Stop
  case 0x09: // Pause
    cdOf(c).stopClip();
    break;
  default:
    break;
  }
  if (cmd == 0x10 && result) { // GetlocL
    const std::uint32_t task = c->mem_r32(sched::kCurrentTaskPointer);
    const int sector = static_cast<int>(c->mem_r32(task + 0x54)) + 150; // FUN_8008A00C's LBA->MSF
    const int frame = sector % 75;
    const int seconds = (sector / 75) % 60;
    const int minutes = (sector / 75) / 60;
    c->mem_w8(result + 0, bcd(minutes));
    c->mem_w8(result + 1, bcd(seconds));
    c->mem_w8(result + 2, bcd(frame));
  }
  c->r[V0] = 0;
}

// FUN_8008C1EC(blocks, lba, buf): LBA->MSF, Setloc, ReadN and the blocking wait; v0 = 1 on success.
void cdReadPrimitive(Core *c) {
  c->r[V0] = EngineCdLoader(*c).readBlocks(c->r[A0], c->r[A1], c->r[A2]) ? 1u : 0u;
}

void cdFileLoad(Core *c) {
  EngineCdLoader(*c).loadFile(c->r[A0], c->r[A1], c->r[A2]);
  c->r[V0] = c->r[A2];
}

void cdAsyncRead(Core *c) {
  EngineCdLoader(*c).asyncRead();
}

void cdInlineLoad(Core *c) {
  EngineCdLoader(*c).dc40Sync({.dest = c->r[A0], .lba = c->r[A1], .size = c->r[A2]});
}

// FUN_8001D2A8(chan, start, end, flags): plays a clip on the native XA streamer and holds slot 2 until
// it ends. A looping clip is area music: held back while a dialog tone plays, faded in otherwise.
void voicePlay(Core *c) {
  const auto channel = static_cast<std::uint8_t>(c->r[A0] & 0xFFu);
  const std::uint32_t start = c->r[A1];
  const std::uint32_t end = c->r[A2];
  const bool loop = (c->r[A3] & 1u) != 0;
  lucent::debug("voice_play", "chan={} [{}..{}] loop={} ra={:08X}", channel, start, end, loop, c->r[31]);
  Engine &engine = eng(c);
  if (loop) {
    cdOf(c).deferredMusic() = {.pending = true, .channel = channel, .start = start, .end = end};
    if (engine.musicCoord.dialogToneActive()) {
      return;
    }
  }
  cdOf(c).playClip({.channel = channel, .start = start, .end = end, .loop = loop});
  if (loop) {
    engine.musicCoord.musicFadeIn();
  }
}

// FUN_8001CF2C: stops the clip. Outside a dialog it also forgets deferred music, so a menu exit does
// not replay the clip it just stopped.
void voiceStop(Core *c) {
  cdOf(c).stopClip();
  c->mem_w16(kVoiceTaskState, 0);
  if (!eng(c).musicCoord.dialogToneActive()) {
    cdOf(c).deferredMusic().pending = false;
  }
  c->r[A0] = 0;
  psx::cpu::dispatchGuestToReturn0(*c, kCdToSpuMix, psx::cpu::ExecutionBudget::currentTurn(*c), "voiceStop");
}

} // namespace

void EngineCd::declareOverrides(native::NativeOverrideCatalog &natives) {
  natives.declare(kCdCmdStream, "EngineCd::commandStream", cdCommandStream);
  natives.declare(kCdReadPrimitive, "EngineCdLoader::readBlocks", cdReadPrimitive);
  natives.declare(kCdFileLoad, "EngineCdLoader::loadFile", cdFileLoad);
  natives.declare(kCdAsyncRead, "EngineCdLoader::asyncRead", cdAsyncRead);
  natives.declare(kCdInlineLoad, "EngineCdLoader::dc40Sync", cdInlineLoad);
  natives.declare(kVoicePlay, "EngineCd::voicePlay", voicePlay);
  natives.declare(kVoiceStop, "EngineCd::voiceStop", voiceStop);
}

void EngineCd::playClip(const Clip &clip) {
  psx::audio::XaStream *xa = &core_.game->xa;
  const bool streaming =
      holdsVoiceSlot_ && xa->isActive() != 0 && clip.channel == clip_.channel && clip.start == clip_.start;
  if (!streaming) {
    xa->play(clip.channel, clip.start, clip.end, clip.loop ? 1 : 0);
    clip_ = clip;
  }
  holdsVoiceSlot_ = true;
  core_.mem_w16(kVoiceTaskState, static_cast<std::uint16_t>(sched::TaskState::Runnable));
  toSpuMix(true);
}

void EngineCd::stopClip() {
  core_.game->xa.stop();
  holdsVoiceSlot_ = false;
}

bool EngineCd::clipStreaming() const {
  return holdsVoiceSlot_ && core_.game->xa.isActive() != 0;
}

void EngineCd::toSpuMix(bool on) {
  core_.r[A0] = on ? 1 : 0;
  psx::cpu::dispatchGuestToReturn0(
      core_, kCdToSpuMix, psx::cpu::ExecutionBudget::currentTurn(core_), "EngineCd::toSpuMix");
}

void EngineCd::hleInit() {
  for (std::size_t i = 0; i < kCdCallbackSlots.size(); ++i) {
    core_.mem_w32(kCdCallbackSlots[i], kCdCallbacks[i]);
  }
  lucent::debug("cd", "CdInit: callbacks installed, no controller handshake");
}

// tgt/cur/mas: the CD fade (0x800BE222/224/220); 19a/137 gate FUN_80075824's ramp; song 0x800BED80.
void EngineCd::audioTrace(const char *tag) {
  if (!traceChannel_) {
    return;
  }
  const std::array<int, 9> now{core_.mem_r16s(0x800BE222u),
                               core_.mem_r16s(0x800BE224u),
                               core_.mem_r16s(0x800BE220u),
                               static_cast<int>(core_.mem_r8(0x1F80019Au)),
                               static_cast<int>(core_.mem_r8(0x1F800137u)),
                               static_cast<int>(core_.mem_r16(0x800BED80u)),
                               core_.game->xa.isActive(),
                               core_.game->xa.isLooping(),
                               static_cast<int>(core_.mem_r16(kVoiceTaskState))};
  if (now == traced_) {
    return;
  }
  lucent::debug(traceChannel_,
                "[xa f{} {:<5}] tgt={} cur={} mas={} 19a={} 137={} song={} act={} loop={} gate={}",
                core_.game->timing.logicFrame,
                tag,
                now[0],
                now[1],
                now[2],
                now[3],
                now[4],
                now[5],
                now[6],
                now[7],
                now[8]);
  traced_ = now;
}

} // namespace tomba
