#include "entry/tomba_catalog.h"

#include "entry/tomba_identity.h"

#include <array>
#include <cstdlib>
#include <lucent/log.h>

namespace tomba {
namespace {

const std::array<psx::host::TitleIdentity, 1> kIdentities{title::bootStubIdentity()};

} // namespace

std::string_view TombaCatalog::productName() const {
  return "Tomba!";
}

std::span<const psx::host::TitleIdentity> TombaCatalog::titles() const {
  return kIdentities;
}

TombaRuntime &TombaCatalog::runtime(std::size_t index) const {
  if (index >= kIdentities.size()) {
    lucent::error("tomba-catalog", "no runtime for catalog index {}", index);
    std::abort();
  }
  return runtime_;
}

} // namespace tomba
