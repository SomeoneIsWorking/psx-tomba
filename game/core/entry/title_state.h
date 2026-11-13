// title_state.h — Tomba! 2's save-state section: the boot cards' progress, the area music EngineCd holds back
// for a dialog and the scheduler's host half of the task slots. Overlay natives are rebound to the restored images.
#pragma once

#include "core/frame/boot_cards.h"
#include "core/sched/scheduler.h"
#include "machine_state.h"

namespace tomba {

class DrawReplay;
class EngineCd;

class TitleState final : public psx::state::NativeStatePort {
public:
  TitleState(EngineCd &cd, sched::Scheduler &scheduler, DrawReplay &replay, BootCards &cards)
      : cd_(cd), scheduler_(scheduler), replay_(replay), cards_(cards) {}

  // Renaming the section is a format change.
  [[nodiscard]] const char *sectionName() const override {
    return "tomba2.cd";
  }
  [[nodiscard]] std::uint32_t version() const override {
    return 3;
  }
  // Refuses while a guest task is parked on a fiber (its host stack cannot be saved) or the LOGO movie plays.
  bool save(psx::platform::BlobWriter &out, std::string &error) const override;
  bool load(psx::platform::BlobReader &in, std::string &error) override;
  // Adopts the staged cards and slots; the restored frame does not follow the last one drawn.
  void restored(Core &core) override;

private:
  EngineCd &cd_;
  sched::Scheduler &scheduler_;
  DrawReplay &replay_;
  BootCards &cards_;
  BootCards::Progress stagedCards_{};
  sched::Scheduler::SlotStates stagedSlots_{};
};

} // namespace tomba
