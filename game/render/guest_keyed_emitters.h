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
//   A03 0x801113B4  leaf: the node's ribbon through 0x80110B00, ten segments of four G4 quads at bucket 4 between
//                   the 11 screen points at +0x3C, unculled.
// A leaf handed screen points is one object per call, by its call site and n-th call in the logic frame:
//   A06 0x80129AF8  a semi-transparent three-point line from a1 through the midpoint to a0, colour a2, bucket a3,
//                   drawn by the node handlers 0x80129C50 (twice) and 0x8012A564.
// A body that takes no object draws one screen:
//   DEMO 0x80106690  the front end's two edge sprites (x 0 and 0x100, clut 0x7FE8) into the pool.
//   A01  0x801311FC  a per-frame effect: a texture-window packet and its GT4 particles.
//   A03  0x80109EEC  the area's HUD, sprites at bucket 3, drawn by 0x8010B328 before the ground.
#pragma once

#include "state_producer.h"

#include <cstdint>

class Core;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba::render {

// What names the object a guest emitter's call draws.
enum class EmitterKey : std::uint8_t {
  Node,     // the node in a0
  CallSite, // its call site and n-th call
  Screen,   // one screen
};

struct GuestKeyedEmitter {
  const char *overlay;
  std::uint32_t emitter;
  EmitterKey key;
};

class GuestKeyedEmitters {
public:
  static constexpr GuestKeyedEmitter kEmitters[] = {
      {"MAIN", 0x800328BCu, EmitterKey::Node},
      {"A01", 0x80129114u, EmitterKey::Node},
      {"A01", 0x8012D9E8u, EmitterKey::Node},
      {"A00", 0x8013E08Cu, EmitterKey::Node},
      {"A07", 0x80117D4Cu, EmitterKey::Node},
      {"A0F", 0x80116ABCu, EmitterKey::Node},
      {"A0F", 0x801143C4u, EmitterKey::Node},
      {"A0D", 0x80110C14u, EmitterKey::Node},
      {"A03", 0x801113B4u, EmitterKey::Node},
      {"A06", 0x80129AF8u, EmitterKey::CallSite},
      {"DEMO", 0x80106690u, EmitterKey::Screen},
      {"A01", 0x801311FCu, EmitterKey::Screen},
      {"A03", 0x80109EECu, EmitterKey::Screen},
  };

  static void registerOverrides(native::NativeOverrideCatalog &natives);
  static void registerStateRenders(psx::present::StateProducerTable &renders);
};

} // namespace tomba::render
