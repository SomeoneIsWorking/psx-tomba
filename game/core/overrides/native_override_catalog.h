#pragma once

#include "native_dispatch.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

class Core;

namespace tomba::native {

// Records a title-owned native implementation without binding it to an address-only global table.
// Declarations are process-lifetime metadata; binding requires an explicit resident-image token.
// `producer` names the argument register of the drawn object when the function writes guest packets.
void declareOverride(std::uint32_t address,
                     std::string_view name,
                     psx::cpu::NativeFunction function,
                     std::optional<psx::present::Producer> producer = std::nullopt);

// Records an implementation that belongs to one named runtime overlay. Overlay declarations may
// share a numeric address with resident or other-overlay declarations; the image name is part of
// the ownership key and is checked again when the image is bound.
void declareOverlayOverride(std::string_view imageName,
                            std::uint32_t address,
                            std::string_view name,
                            psx::cpu::NativeFunction function,
                            std::optional<psx::present::Producer> producer = std::nullopt);

// Installs resident declarations only when the supplied image still owns the address. The caller
// obtains the token at resident load, never by adopting an arbitrary active overlay. Repeated calls
// are idempotent. This is a residency contract, not executable-content authentication.
void bindResident(Core &core, psx::cpu::ImageIdentity resident, GuestAddressRange residentText);

// Activates the bytes already loaded in `text`, retires the previous image in the same MODE slot,
// and binds declarations whose overlay name and address range match. The content identity is
// derived from the loaded bytes here so callers cannot accidentally publish a stale or fabricated
// token. `active` is per-Core state owned by the MODE loader.
psx::cpu::ImageIdentity activateOverlay(Core &core,
                                        std::optional<psx::cpu::ImageIdentity> &active,
                                        std::string_view imageName,
                                        GuestAddressRange text);

// Retire a loaded code image before its slot is reused or returns to resident code.
void retireOverlay(Core &core, std::optional<psx::cpu::ImageIdentity> &active);

// Publishes an already-loaded image in the fixed MODE slot. File index 2 is SOP; indices 3..24
// are A00..A0L. Read the size from FUN_80045080's own descriptor table after its load completes.
psx::cpu::ImageIdentity
activateModeOverlay(Core &core, std::optional<psx::cpu::ImageIdentity> &active, std::uint32_t fileIndex);

// Publishes OPN (index 0) or CRD (index 1) already loaded at 0x8018A000.
// The caller retires this token before that shared slot is reused for raw area or texture data.
psx::cpu::ImageIdentity
activateAreaSlotOverlay(Core &core, std::optional<psx::cpu::ImageIdentity> &active, std::uint32_t fileIndex);

// The native owner of guest FUN_80045558(idx) for every caller: retires whatever image the shared
// AREA slot at 0x8018A000 holds, reads indexed file `fileIndex` (0 = OPN, 1 = CRD) into it
// synchronously, and publishes the loaded code image. Returns the loaded size, FUN_80045558's v0.
std::uint32_t loadAreaSlotFile(Core &core, std::optional<psx::cpu::ImageIdentity> &active, std::uint32_t fileIndex);

// The disc's 22 field-code files are named A00..A0L in area-index order.
std::string overlayNameForArea(std::uint32_t area);

} // namespace tomba::native
