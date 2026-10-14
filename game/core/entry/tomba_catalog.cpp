#include "entry/tomba_catalog.h"

#include "entry/tomba_identity.h"
#include "tomba1_identity.h"

#include <array>
#include <cstdlib>
#include <lucent/log.h>

namespace tomba {
namespace {

constexpr std::size_t kTomba1 = 0;
constexpr std::size_t kTomba2 = 1;

const std::array<psx::host::TitleIdentity, 2> kIdentities{tomba1::identity(), title::bootStubIdentity()};

} // namespace

std::string_view TombaCatalog::productName() const {
  return "Tomba!";
}

std::filesystem::path TombaCatalog::provisioningRoot() const {
  return "scratch/bin";
}

std::span<const psx::host::TitleIdentity> TombaCatalog::titles() const {
  return kIdentities;
}

GameRuntime &TombaCatalog::runtime(std::size_t index) const {
  switch (index) {
  case kTomba1:
    return tomba1_;
  case kTomba2:
    return tomba2_;
  default:
    lucent::error("tomba-catalog", "no runtime for catalog index {}", index);
    std::abort();
  }
}

std::unique_ptr<psx::host::TitleCatalog> createCatalog() {
  return std::make_unique<TombaCatalog>();
}

} // namespace tomba
