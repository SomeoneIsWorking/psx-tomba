#pragma once

#include "native_dispatch.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class Core;

namespace tomba::native {

// The title's native implementations, declared once per Game and bound to each image the Core loads.
// Declarations key on address, or on image name plus address where overlays reuse a slot.
class NativeOverrideCatalog {
public:
  // `producer` names the argument register of the drawn object when the function writes guest packets.
  void declare(std::uint32_t address,
               std::string_view name,
               psx::cpu::NativeFunction function,
               std::optional<psx::present::Producer> producer = std::nullopt);

  // Overlay declarations may share an address with resident or other-overlay ones; the image name is
  // part of the key and is checked again when the image is bound.
  void declareOverlay(std::string_view imageName,
                      std::uint32_t address,
                      std::string_view name,
                      psx::cpu::NativeFunction function,
                      std::optional<psx::present::Producer> producer = std::nullopt);

  // Installs resident declarations only where the supplied image still owns the address. Idempotent.
  void bindResident(Core &core, psx::cpu::ImageIdentity resident, GuestAddressRange residentText) const;

  // Activates the bytes already loaded in `text`, retires the previous image held in `active`, and
  // binds the declarations naming this image. The content identity is derived from the loaded bytes.
  psx::cpu::ImageIdentity activateOverlay(Core &core,
                                          std::optional<psx::cpu::ImageIdentity> &active,
                                          std::string_view imageName,
                                          GuestAddressRange text) const;

  // Retire a loaded code image before its slot is reused or returns to resident code.
  void retireOverlay(Core &core, std::optional<psx::cpu::ImageIdentity> &active) const;

  // Publishes an already-loaded image in the MODE slot. File index 2 is SOP; 3..24 are A00..A0L.
  psx::cpu::ImageIdentity
  activateModeOverlay(Core &core, std::optional<psx::cpu::ImageIdentity> &active, std::uint32_t fileIndex) const;

  // Publishes OPN (index 0) or CRD (index 1) already loaded at 0x8018A000.
  psx::cpu::ImageIdentity
  activateAreaSlotOverlay(Core &core, std::optional<psx::cpu::ImageIdentity> &active, std::uint32_t fileIndex) const;

  // FUN_80045558(idx): reads OPN or CRD into the AREA slot synchronously and publishes it. Returns the size.
  std::uint32_t
  loadAreaSlotFile(Core &core, std::optional<psx::cpu::ImageIdentity> &active, std::uint32_t fileIndex) const;

private:
  struct Declaration {
    std::string imageName;
    std::uint32_t address = 0;
    std::string name;
    psx::cpu::NativeFunction function = nullptr;
    std::optional<psx::present::Producer> producer;
  };

  void bindOverlay(Core &core,
                   psx::cpu::ImageIdentity overlay,
                   std::string_view imageName,
                   GuestAddressRange overlayText) const;

  std::vector<Declaration> declarations_;
};

// The disc's 22 field-code files are named A00..A0L in area-index order.
std::string overlayNameForArea(std::uint32_t area);

} // namespace tomba::native
