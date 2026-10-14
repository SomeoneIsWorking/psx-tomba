// tomba_cd_state.h — Tomba! 2's save-state section: the area music TombaCd holds back for a dialog.
#pragma once

#include "machine_state.h"

namespace tomba {

class TombaCd;

class TombaCdState final : public psx::state::NativeStatePort {
public:
  explicit TombaCdState(TombaCd &cd) : cd_(cd) {}

  [[nodiscard]] const char *sectionName() const override {
    return "tomba2.cd";
  }
  [[nodiscard]] std::uint32_t version() const override {
    return 1;
  }
  bool save(psx::state::BlobWriter &out, std::string &error) const override;
  bool load(psx::state::BlobReader &in, std::string &error) override;

private:
  TombaCd &cd_;
};

} // namespace tomba
