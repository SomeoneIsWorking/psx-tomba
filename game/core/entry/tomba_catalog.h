#pragma once

#include "entry/tomba_runtime.h"
#include "title_catalog.h"

#include <span>

namespace tomba {

// The multi-title host's catalog. Tomba! 1 joins it when its product runs past the guest task fault at
// 0x800E7D5C (titles/tomba1/docs/issues, 0008); until then it aborts the process in its first second.
class TombaCatalog final : public psx::host::TitleCatalog {
public:
  std::string_view productName() const override;
  std::span<const psx::host::TitleIdentity> titles() const override;
  TombaRuntime &runtime(std::size_t index) const override;

private:
  // The host asks for a mutable runtime from a const catalog; the runtime is the process's one.
  mutable TombaRuntime runtime_;
};

} // namespace tomba
