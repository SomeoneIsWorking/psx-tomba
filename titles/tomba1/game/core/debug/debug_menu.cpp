#include "debug_menu.h"

#include "context.h"
#include "core.h"
#include "dev_warp.h"

#include <lucent/log.h>

#include <vector>

namespace tomba1 {

DebugMenu::DebugMenu() {
  warp({
      .levels = {"area", "section"},
      .places =
          [](Core &core) {
            std::vector<psx::game::WarpPlace> areas;
            if (!DevWarp::fieldRunning(core)) {
              return areas;
            }
            for (int area = 0; area < DevWarp::areaCount(core); ++area) {
              psx::game::WarpPlace place{.name = lucent::format("area {}", area)};
              const std::uint32_t sections =
                  core.mem_r16(DevWarp::kSectionCounts + static_cast<std::uint32_t>(area) * 2u);
              for (std::uint32_t section = 0; section < sections; ++section) {
                place.places.push_back({
                    .name = lucent::format("section {}", section),
                    .arrive =
                        [area, section](Core &target) {
                          return context(target).warp.arm(target, static_cast<std::uint32_t>(area), section, 0u);
                        },
                });
              }
              if (!place.places.empty()) {
                areas.push_back(std::move(place));
              }
            }
            return areas;
          },
      .refusal =
          [](Core &core) {
            return DevWarp::fieldRunning(core) ? std::string() : "a warp is legal only while the field is running";
          },
  });
}

} // namespace tomba1
