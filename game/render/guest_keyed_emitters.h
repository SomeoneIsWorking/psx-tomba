// game/render/guest_keyed_emitters.h — guest render bodies the title has not ported, keyed to an owner.
//
// An area overlay's render leaf, or the front end's chrome drawer, draws through guest bodies only that
// image has, so nothing keys its packets and every record holding one is shown whole. Each is wrapped: its
// packets are saved as its owner's state and the render moves their positions by t (quad_state.h).
//
// The node in a0 owns a leaf that draws one object:
//   MAIN 0x800328BC  the node's sprite stamp: 0x80027A4C over the records in a1 with the node's texture word at +0x44,
//                    called straight from the area overlays' effect leaves.
//   A01 0x80129114  leaf: stages the node's angle and calls FUN_80128B08.
//   A01 0x8012D9E8  leaf: composes the node's transform and emits its record list.
//   A00 0x8013E08C  leaf: an untextured semi-transparent line strip from the node's points.
//   A07 0x80117D4C  leaf: the node's sagging strip of 16 points through 0x80117AB8.
//   A0F 0x80116ABC  leaf: the node's wire, two hanging sides and a foot, as 0x80112D10 shimmer lines.
//   A0F 0x801143C4  leaf: the node's lens flare, three quads toward screen centre, and its 0x80027A4C sparkles.
//   A0D 0x80110C14  leaf: the node's ring of 21 0x80027A4C stamps.
// A body that takes no object draws one screen:
//   DEMO 0x80106690  the front end's two edge sprites (x 0 and 0x100, clut 0x7FE8) into the pool.
//   A01  0x801311FC  a per-frame effect: a texture-window packet and its GT4 particles.
#pragma once

#include "state_producer.h"

#include <cstdint>

class Core;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba::render {

struct GuestKeyedEmitter {
  const char *overlay;
  std::uint32_t emitter;
  bool nodeOwned; // the node in a0 owns it; otherwise it draws one screen
};

class GuestKeyedEmitters {
public:
  static constexpr GuestKeyedEmitter kEmitters[] = {
      {"MAIN", 0x800328BCu, true},
      {"A01", 0x80129114u, true},
      {"A01", 0x8012D9E8u, true},
      {"A00", 0x8013E08Cu, true},
      {"A07", 0x80117D4Cu, true},
      {"A0F", 0x80116ABCu, true},
      {"A0F", 0x801143C4u, true},
      {"A0D", 0x80110C14u, true},
      {"DEMO", 0x80106690u, false},
      {"A01", 0x801311FCu, false},
  };

  static void registerOverrides(native::NativeOverrideCatalog &natives);
  static void registerStateRenders(psx::present::StateProducerTable &renders);
};

} // namespace tomba::render
