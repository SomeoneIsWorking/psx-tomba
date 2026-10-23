// game/ui/panel.h — class Panel, the 2D UI panel emitters Tomba! 2 owns natively.
// FUN_8005019C build, FUN_8004FFB4 fillQuad, FUN_8007FCC8 pushDialogBackdrop.
#pragma once
#include "emit_memory.h"
#include "state_producer.h"

#include <cstdint>
class Core;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba::render {
class OrderingTable;
struct ListCall;
} // namespace tomba::render

namespace tomba {

class Panel {
public:
  static constexpr std::uint32_t kBuild = 0x8005019Cu;
  static constexpr std::uint32_t kFill = 0x8004FFB4u;

  // Registers FUN_8005019C and FUN_8004FFB4. Idempotent.
  static void install(native::NativeOverrideCatalog &natives);
  static void registerStateRenders(psx::present::StateProducerTable &renders, Core &core);

  struct PushDialogBackdropArgs {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    uint32_t mode;
  };
  // FUN_8007FCC8: one GP0 0x60 packet into the near or far bucket; `mode` 0x80 selects far, any of 0x7F means no fill.
  static void pushDialogBackdrop(Core *c, const PushDialogBackdropArgs &args);

  struct FillArgs {
    std::uint32_t rect; // s16 x, y, w, h
    std::uint32_t uvIndex;
    std::uint16_t attr;
    std::uint32_t bucket;
  };
  // FUN_8004FFB4's body: GP0 0x2C/0x2E textured quad with attr-driven CLUT, flat shading and semi-transparency.
  static void fill(const psx::present::EmitMemory &memory, const render::OrderingTable &ot, const FillArgs &args);
  static void fillQuad(Core *c);

  // FUN_8005019C's body: `call.list` the rect, `a1` the style, `count` the shadow flag, `a3` the bucket.
  static void build(const psx::present::EmitMemory &memory, const render::ListCall &call);
  static void drawPanel(Core *c);
};

} // namespace tomba
