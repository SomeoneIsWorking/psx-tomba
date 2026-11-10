// title_state.h — Tomba! 2's save-state section: the area music EngineCd holds back for a dialog.
#pragma once

#include "machine_state.h"

namespace tomba {

class DrawReplay;
class EngineCd;

class TitleState final : public psx::state::NativeStatePort {
public:
  TitleState(EngineCd &cd, DrawReplay &replay) : cd_(cd), replay_(replay) {}

  // Renaming the section is a format change.
  [[nodiscard]] const char *sectionName() const override {
    return "tomba2.cd";
  }
  [[nodiscard]] std::uint32_t version() const override {
    return 1;
  }
  bool save(psx::platform::BlobWriter &out, std::string &error) const override;
  bool load(psx::platform::BlobReader &in, std::string &error) override;
  // The restored frame does not follow the last one drawn.
  void restored(Core &core) override;

private:
  EngineCd &cd_;
  DrawReplay &replay_;
};

} // namespace tomba
