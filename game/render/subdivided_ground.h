// game/render/subdivided_ground.h — A03's GT4 list 0x80110290 under the drawer 0x801109D4: the ground, split up
// near the camera.
//
// A record nearer than SZ 0x384 at every corner is dropped. Flag bit 0 clear: under SZ 0xDAC the projected quad is
// faded toward the far colour by 0xDAC - SZ and split on screen into four (0x8010F968); past it the quad is averaged,
// pushed 0x5A buckets back and fogged by SZ - 0x1D4C past bucket 0x600. Flag bit 0 set: under SZ 0xDAC the model quad
// is split twice into sixteen and each projected again (0x8010F1A0, black at corners nearer than SZ 0x514); under
// 0x157C it is split on screen unfaded; past that averaged and fogged. The guest keeps a quad for any SX < 320; the
// cut is the draw window.
#pragma once

#include "state_producer.h"

#include "emit_memory.h"
#include "list_job.h"

#include <cstdint>

struct Core;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba::horizontal_cull {
class Visibility;
}

namespace tomba::render {

class SubdividedGround {
public:
  // (a0 = record list, a1 = OT base, a2 = count); the end of the list in v0.
  static void draw(Core *c);
  // The body over any memory; returns the address past the list.
  static std::uint32_t
  emit(const psx::present::EmitMemory &memory, const ListCall &call, const horizontal_cull::Visibility &visible);

  static void registerOverrides(native::NativeOverrideCatalog &natives);
  static void registerStateRenders(psx::present::StateProducerTable &renders, Core &core);
};

} // namespace tomba::render
