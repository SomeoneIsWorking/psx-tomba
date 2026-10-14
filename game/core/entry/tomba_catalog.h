#pragma once

#include "entry/tomba_runtime.h"
#include "title_catalog.h"
#include "tomba1_runtime.h"

#include <filesystem>
#include <memory>
#include <span>

namespace tomba {

// The product's catalog: Tomba! 1 (titles/tomba1) and Tomba! 2 (game/), one picker panel each.
class TombaCatalog final : public psx::host::TitleCatalog {
public:
  std::string_view productName() const override;
  std::filesystem::path provisioningRoot() const override;
  std::span<const psx::host::TitleIdentity> titles() const override;
  GameRuntime &runtime(std::size_t index) const override;

private:
  // The host asks for a mutable runtime from a const catalog; each runtime is its title's one.
  mutable tomba1::Tomba1Runtime tomba1_;
  mutable TombaRuntime tomba2_;
};

// The product's catalog, run by the framework main (psxport_add_product).
std::unique_ptr<psx::host::TitleCatalog> createCatalog();

} // namespace tomba
