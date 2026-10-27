#include "debug_menu.h"

#include "context.h"
#include "core.h"
#include "dev_warp.h"

namespace tomba1 {

DebugMenu::DebugMenu() {
  using psx::game::DebugCall;
  using psx::game::DebugParam;
  section("Area Warp");
  verb({
      .name = "warp",
      .summary = "Warp there through the door transition",
      .params = {DebugParam::named("area",
                                   {
                                       [](Core &core) {
                                         return DevWarp::areaCount(core);
                                       },
                                       [](Core &, int) {
                                         return std::string();
                                       },
                                   }),
                 DebugParam::integer("section", 0, 0xFF).orAbsent(),
                 DebugParam::integer("entry", 0, DevWarp::kMaxEntry).orAbsent()},
      .run =
          [](const DebugCall &call) {
            call.reply =
                context(call.core).warp.arm(call.core, call.values.u32(0), call.values.u32(1), call.values.u32(2));
          },
      .refusal =
          [](Core &core) {
            return DevWarp::fieldRunning(core) ? std::string() : "a warp is legal only while the field is running";
          },
  });
}

} // namespace tomba1
