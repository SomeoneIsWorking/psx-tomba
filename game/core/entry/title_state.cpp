#include "core/entry/title_state.h"

#include "blob_codec.h"
#include "cd/engine_cd.h"
#include "core/frame/draw_replay.h"
#include "core/overrides/native_override_catalog.h"
#include "entry/session.h"
#include "scene/level_load.h"

#include <string>

namespace tomba {
namespace {

void writeContext(psx::platform::BlobWriter &out, const R3000 &context) {
  for (const std::uint32_t reg : context.r) {
    out.u32(reg);
  }
  out.u32(context.hi);
  out.u32(context.lo);
  out.u32(context.pc);
}

R3000 readContext(psx::platform::BlobReader &in) {
  R3000 context{};
  for (std::uint32_t &reg : context.r) {
    reg = in.u32();
  }
  context.hi = in.u32();
  context.lo = in.u32();
  context.pc = in.u32();
  return context;
}

} // namespace

bool TitleState::save(psx::platform::BlobWriter &out, std::string &error) const {
  if (const int slot = scheduler_.slotOnFiber(); slot >= 0) {
    error = "tomba2.cd: task slot " + std::to_string(slot) + " is parked on a host fiber";
    return false;
  }
  const EngineCd::DeferredMusic &music = cd_.deferredMusic();
  out.boolean(music.pending);
  out.u8(music.channel);
  out.u32(music.start);
  out.u32(music.end);
  for (const sched::Scheduler::SlotState &slot : scheduler_.slotStates()) {
    out.boolean(slot.started);
    out.boolean(slot.demoNative);
    out.boolean(slot.gameNative);
    out.boolean(slot.gameCoop);
    writeContext(out, slot.context);
  }
  return true;
}

bool TitleState::load(psx::platform::BlobReader &in, std::string &error) {
  EngineCd::DeferredMusic music;
  music.pending = in.boolean();
  music.channel = in.u8();
  music.start = in.u32();
  music.end = in.u32();
  sched::Scheduler::SlotStates slots;
  for (sched::Scheduler::SlotState &slot : slots) {
    slot.started = in.boolean();
    slot.demoNative = in.boolean();
    slot.gameNative = in.boolean();
    slot.gameCoop = in.boolean();
    slot.context = readContext(in);
  }
  if (!in.ok()) {
    error = "tomba2.cd: the deferred music or task slots are truncated";
    return false;
  }
  cd_.deferredMusic() = music;
  stagedSlots_ = slots;
  return true;
}

void TitleState::restored(Core &core) {
  scheduler_.adoptSlotStates(stagedSlots_);
  // Overlay natives are bound to images, which the restore re-activated under new identities.
  Session &session = sessionOf(&core);
  Engine &engine = session.engine;
  session.nativeOverrides.adoptRestoredOverlay(core, engine.activeStageOverlay, stage::kSlot);
  session.nativeOverrides.adoptRestoredOverlay(core, engine.activeModeOverlay, native::kModeSlot);
  session.nativeOverrides.adoptRestoredOverlay(core, engine.activeAreaOverlay, native::kAreaSlot);
  replay_.forget();
}

} // namespace tomba
