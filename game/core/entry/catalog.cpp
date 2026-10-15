#include "entry/catalog.h"

#include <array>
#include <cstdlib>
#include <lucent/log.h>

namespace tomba {
namespace {

constexpr std::size_t kTomba1 = 0;
constexpr std::size_t kTomba2 = 1;

} // namespace

std::string_view Catalog::productName() const {
  return "Tomba!";
}

std::filesystem::path Catalog::provisioningRoot() const {
  return "scratch/bin";
}

std::span<const psx::host::TitleIdentity> Catalog::titles() const {
  return identities_;
}

psx::game::GameRuntime &Catalog::runtime(std::size_t index) const {
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
  return std::make_unique<Catalog>();
}

} // namespace tomba
