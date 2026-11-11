#include "frame/image_residency.h"

#include "core.h"
#include "image_identity.h"

#include <optional>

namespace tomba {

bool imageHolds(const Core &core, std::uint32_t address, std::string_view image) {
  const psx::cpu::ImageCatalog &images = core.imageCatalog();
  const std::optional<psx::cpu::ImageIdentity> identity = images.resolve(address);
  if (!identity) {
    return false;
  }
  const std::optional<psx::cpu::ImageDescription> description = images.describe(*identity);
  return description && description->name == image;
}

void ResidentImage::sample(Core &core) {
  resident_ = imageHolds(core, code_, image_);
}

} // namespace tomba
