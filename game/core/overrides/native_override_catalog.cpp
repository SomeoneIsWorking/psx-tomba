#include "overrides/native_override_catalog.h"

#include "core.h"
#include "game.h" // Cd::dc40Sync — the synchronous indexed-file reader

#include <algorithm>
#include <cstdlib>
#include <lucent/log.h>
#include <string>
#include <utility>
#include <vector>

namespace tomba::native {
namespace {

struct Declaration {
  std::string imageName;
  std::uint32_t address = 0;
  std::string name;
  psx::cpu::NativeFunction function = nullptr;
  std::optional<psx::present::Producer> producer;
};

std::vector<Declaration> &declarations() {
  static std::vector<Declaration> value;
  return value;
}

bool sameProducer(const std::optional<psx::present::Producer> &a, const std::optional<psx::present::Producer> &b) {
  return a.has_value() == b.has_value() && (!a || a->object == b->object);
}

} // namespace

void declareOverride(std::uint32_t address,
                     std::string_view name,
                     psx::cpu::NativeFunction function,
                     std::optional<psx::present::Producer> producer) {
  if (address == 0u || name.empty() || function == nullptr) {
    lucent::error("tomba-native", "refused incomplete native override declaration at 0x{:08X}", address);
    std::abort();
  }

  auto &entries = declarations();
  const auto existing = std::find_if(entries.begin(), entries.end(), [address](const Declaration &entry) {
    return entry.imageName.empty() && entry.address == address;
  });
  if (existing != entries.end()) {
    if (existing->function == function && existing->name == name && sameProducer(existing->producer, producer)) {
      return;
    }
    lucent::error("tomba-native",
                  "refused conflicting native overrides '{}' and '{}' at 0x{:08X}",
                  existing->name,
                  name,
                  address);
    std::abort();
  }
  entries.push_back({{}, address, std::string(name), function, producer});
}

void declareOverlayOverride(std::string_view imageName,
                            std::uint32_t address,
                            std::string_view name,
                            psx::cpu::NativeFunction function,
                            std::optional<psx::present::Producer> producer) {
  if (imageName.empty() || address == 0u || name.empty() || function == nullptr) {
    lucent::error("tomba-native", "refused incomplete overlay override declaration at 0x{:08X}", address);
    std::abort();
  }

  auto &entries = declarations();
  const auto existing = std::find_if(entries.begin(), entries.end(), [&](const Declaration &entry) {
    return entry.imageName == imageName && entry.address == address;
  });
  if (existing != entries.end()) {
    if (existing->function == function && existing->name == name && sameProducer(existing->producer, producer)) {
      return;
    }
    lucent::error("tomba-native",
                  "refused conflicting '{}' overlay overrides '{}' and '{}' at 0x{:08X}",
                  imageName,
                  existing->name,
                  name,
                  address);
    std::abort();
  }
  entries.push_back({std::string(imageName), address, std::string(name), function, producer});
}

void bindResident(Core &core, psx::cpu::ImageIdentity resident, GuestAddressRange residentText) {
  std::size_t installed = 0;
  std::size_t retained = 0;
  std::size_t inactive = 0;
  std::size_t unreachable = 0;
  for (const Declaration &entry : declarations()) {
    // A RESIDENT declaration outside the resident text range can never install, in any generation:
    // the address belongs to an overlay slot and only declareOverlayOverride can reach it. It is
    // always a mistake, so it is refused by name.
    if (entry.imageName.empty() && !residentText.containsPhysical(entry.address)) {
      lucent::error("tomba-native",
                    "UNREACHABLE: '{}' is declared as a RESIDENT override at 0x{:08X}, outside the "
                    "resident text range. That address is in an overlay slot, so only "
                    "declareOverlayOverride with the owning image name can reach it: this declaration "
                    "can never install and its native behaviour never runs.",
                    entry.name,
                    entry.address);
      ++unreachable;
      continue;
    }
    const auto identity = core.currentImageIdentity(entry.address);
    if (!entry.imageName.empty() || !identity || *identity != resident) {
      ++inactive;
      continue;
    }
    const psx::cpu::NativeKey key{*identity, entry.address};
    if (core.nativeDispatcher().isInstalled(key)) {
      ++retained;
      continue;
    }
    if (!core.nativeDispatcher().install({key, entry.name, entry.function, entry.producer})) {
      lucent::error("tomba-native",
                    "failed to bind '{}' to active image {}:{} at 0x{:08X}",
                    entry.name,
                    identity->id,
                    identity->generation,
                    entry.address);
      std::abort();
    }
    ++installed;
  }
  // A resident declaration outside the resident text range can NEVER install, in any generation: the
  // address belongs to an overlay slot, and only declareOverlayOverride with the owning image name can
  // reach it. So the count is now the abort it should be rather than a log line (issue 0015). It was
  // non-fatal only while conversions were outstanding and an abort would have taken down a product
  // that runs; the list reached zero when the last ten were converted, and this refuses from here.
  //
  // The refusal is the backstop for a resident-form declaration at an address no overlay image owns,
  // including AREA-slot addresses outside the MODE set. `bindResident` cannot install such a
  // declaration, so its native behaviour is simply absent from the product.
  if (unreachable != 0) {
    lucent::error("tomba-native",
                  "{} native override declaration(s) can NEVER install — see the UNREACHABLE lines above "
                  "(issue 0015). Their native behaviour is absent from the product.",
                  unreachable);
    std::abort();
  }
  lucent::info("tomba-native",
               "bound resident image generation: declarations={} installed={} retained={} inactive={}",
               declarations().size(),
               installed,
               retained,
               inactive);
}

void bindOverlay(Core &core,
                 psx::cpu::ImageIdentity overlay,
                 std::string_view imageName,
                 GuestAddressRange overlayText) {
  std::size_t installed = 0;
  std::size_t retained = 0;
  std::size_t inactive = 0;
  std::size_t declaredForImage = 0;
  for (const Declaration &entry : declarations()) {
    if (entry.imageName != imageName) {
      ++inactive;
      continue;
    }
    ++declaredForImage;
    const auto identity = core.currentImageIdentity(entry.address);
    if (!overlayText.containsPhysical(entry.address) || !identity || *identity != overlay) {
      ++inactive;
      continue;
    }
    const psx::cpu::NativeKey key{*identity, entry.address};
    if (core.nativeDispatcher().isInstalled(key)) {
      ++retained;
      continue;
    }
    if (!core.nativeDispatcher().install({key, entry.name, entry.function, entry.producer})) {
      lucent::error("tomba-native",
                    "failed to bind '{}' to active overlay {}:{} at 0x{:08X}",
                    entry.name,
                    identity->id,
                    identity->generation,
                    entry.address);
      std::abort();
    }
    ++installed;
  }
  // `declared` is the DENOMINATOR: without it `installed` cannot be read. A declaration that names this
  // image but does not install is the failure issue 0015 was about — a name that does not own its
  // address, or an address outside the image's loaded text — and `inactive` alone cannot show it,
  // because `inactive` also counts every declaration belonging to a DIFFERENT image. Naming the
  // declared count is what makes "installed=39 declared=39" a verdict rather than a coincidence.
  lucent::info("tomba-native",
               "bound overlay '{}' generation: declarations={} declared_for_image={} installed={} "
               "retained={} inactive={}",
               imageName,
               declarations().size(),
               declaredForImage,
               installed,
               retained,
               inactive);
  if (installed + retained < declaredForImage) {
    lucent::error("tomba-native",
                  "OVERLAY '{}' published {} declaration(s) but installed {}: {} of them name this image "
                  "and could not bind, so their native behaviour is absent while the guest body runs",
                  imageName,
                  declaredForImage,
                  installed + retained,
                  declaredForImage - installed - retained);
  }
}

void retireOverlay(Core &core, std::optional<psx::cpu::ImageIdentity> &active) {
  if (active) {
    for (const Declaration &entry : declarations()) {
      core.nativeDispatcher().remove({*active, entry.address});
    }
    if (!core.imageCatalog().deactivate(*active)) {
      lucent::error(
          "tomba-native", "active overlay image {}:{} disappeared before replacement", active->id, active->generation);
      std::abort();
    }
    active.reset();
  }
}

psx::cpu::ImageIdentity activateOverlay(Core &core,
                                        std::optional<psx::cpu::ImageIdentity> &active,
                                        std::string_view imageName,
                                        GuestAddressRange text) {
  if (imageName.empty() || !text.valid() || text.end > 0x00200000u) {
    lucent::error(
        "tomba-native", "refused invalid overlay image '{}' range [{:08X}, {:08X})", imageName, text.begin, text.end);
    std::abort();
  }
  retireOverlay(core, active);

  std::uint64_t contentIdentity = 1469598103934665603ull;
  for (std::uint32_t address = text.begin; address < text.end; ++address) {
    contentIdentity ^= core.mem_r8(address);
    contentIdentity *= 1099511628211ull;
  }
  const auto identity = core.imageCatalog().activate(imageName, text, contentIdentity);
  bindOverlay(core, identity, imageName, text);
  active = identity;
  return identity;
}

std::string overlayNameForArea(std::uint32_t area) {
  if (area >= 22u) {
    lucent::error("tomba-native", "area {} is outside the A00..A0L overlay set", area);
    std::abort();
  }
  const char suffix = area < 10u ? static_cast<char>('0' + area) : static_cast<char>('A' + area - 10u);
  std::string name = "A0";
  name.push_back(suffix);
  return name;
}

psx::cpu::ImageIdentity
activateModeOverlay(Core &core, std::optional<psx::cpu::ImageIdentity> &active, std::uint32_t fileIndex) {
  constexpr std::uint32_t kModeSlot = 0x00108F9Cu;
  constexpr std::uint32_t kFileTable = 0x800BE118u;
  if (fileIndex < 2u || fileIndex > 24u) {
    lucent::error("tomba-native", "file index {} is outside the SOP/A00..A0L MODE image set", fileIndex);
    std::abort();
  }
  const std::uint32_t size = core.mem_r32(kFileTable + fileIndex * 8u + 4u);
  if (size == 0u || size > 0x00200000u - kModeSlot) {
    lucent::error("tomba-native", "refused invalid MODE file {} overlay size {}", fileIndex, size);
    std::abort();
  }
  const std::string name = fileIndex == 2u ? "SOP" : overlayNameForArea(fileIndex - 3u);
  return activateOverlay(core, active, name, {kModeSlot, kModeSlot + size});
}

namespace {

inline constexpr std::uint32_t kAreaSlot = 0x0018A000u;
inline constexpr std::uint32_t kAreaFileTable = 0x800BE118u; // indexed file table, stride 8 {lba, size}

// The {lba, size} descriptor of AREA-slot file `fileIndex`, refusing anything but OPN/CRD or a size
// the slot cannot hold.
std::pair<std::uint32_t, std::uint32_t> areaSlotFileDescriptor(Core &core, std::uint32_t fileIndex) {
  if (fileIndex > 1u) {
    lucent::error("tomba-native", "file index {} is outside the OPN/CRD AREA image set", fileIndex);
    std::abort();
  }
  const std::uint32_t lba = core.mem_r32(kAreaFileTable + fileIndex * 8u);
  const std::uint32_t size = core.mem_r32(kAreaFileTable + fileIndex * 8u + 4u);
  if (size == 0u || size > 0x00200000u - kAreaSlot) {
    lucent::error("tomba-native", "refused invalid AREA file {} overlay size {}", fileIndex, size);
    std::abort();
  }
  return {lba, size};
}

} // namespace

psx::cpu::ImageIdentity
activateAreaSlotOverlay(Core &core, std::optional<psx::cpu::ImageIdentity> &active, std::uint32_t fileIndex) {
  const auto [lba, size] = areaSlotFileDescriptor(core, fileIndex);
  return activateOverlay(core, active, fileIndex == 0u ? "OPN" : "CRD", {kAreaSlot, kAreaSlot + size});
}

std::uint32_t loadAreaSlotFile(Core &core, std::optional<psx::cpu::ImageIdentity> &active, std::uint32_t fileIndex) {
  const auto [lba, size] = areaSlotFileDescriptor(core, fileIndex);
  retireOverlay(core, active);
  core.game->cd.dc40Sync(0x80000000u | kAreaSlot, lba, size);
  activateAreaSlotOverlay(core, active, fileIndex);
  return size;
}

} // namespace tomba::native
