// class ScreenFade — the guest screen-fade leaf FUN_8007E9C8 and its native callers.
//
// FUN_8007E9C8(color, blend, otSlot) links a full-screen fill (GP0 0x62) and a DR_MODE (tpage 0x40
// subtractive for blend==0, 0x20 additive otherwise) into OT slot `otSlot` from the packet pool
// (guest_ordering_table.h). The guest fill is 320x240 at the origin; the native body spans the draw window
// (wide_window.h), so on the record canvas the margins fade with the buffer. At 4:3 the packets are
// the guest's.
#pragma once
#include "state_producer.h"

#include <cstdint>
class Core;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba::wide_window {
struct Window;
}

namespace tomba {

class ScreenFade {
public:
  static constexpr uint32_t kLeaf = 0x8007E9C8u;
  static constexpr uint32_t kSubtractive = 0;
  static constexpr uint32_t kAdditive = 1;
  // The guest stages the fill's command words here before copying them into the packet.
  static constexpr uint32_t kStage = 0x1F800004u;
  static constexpr int kGuestHeight = 240;

  // Back-pointer set once by Game's constructor.
  Core *core = nullptr;

  // One leaf call: a0 = 0x00BBGGRR fill color, a1 = blend, a2 = OT slot.
  void applyLeafCall(uint32_t color, uint32_t blend, uint32_t otSlot);

  // FUN_8007E9C8 with the guest's register arguments; the fill spans `window`'s columns.
  static void draw(Core &core, const tomba::wide_window::Window &window);
  static void registerOverrides(native::NativeOverrideCatalog &natives);
  // Each call is an object saved as its colour, span and blend; the render moves the colour.
  static void registerStateRenders(psx::present::StateProducerTable &renders);
};

} // namespace tomba
