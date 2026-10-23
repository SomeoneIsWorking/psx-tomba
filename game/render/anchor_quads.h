// game/render/anchor_quads.h — the A00 node render fns that draw textured quads through FUN_8013DD34.
//
// 0x8013E9D8 (rope/anchor) reads two 3-s16 node points and calls FUN_8013DD34 twice; 0x80122974 picks a
// second point from the node, a global or a segment chain and calls it once or nine times. The quads were
// unkeyed, so every record holding one was shown whole; each node now keys them and a render moves the
// quads' screen vertices by t.
#pragma once

#include "state_producer.h"

#include <cstdint>

class Core;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba::render {

class AnchorQuads {
public:
  static constexpr std::uint32_t kEmitters[] = {0x8013E9D8u, 0x80122974u}; // A00

  static void registerOverrides(native::NativeOverrideCatalog &natives);
  static void registerStateRenders(psx::present::StateProducerTable &renders, Core &core);
};

} // namespace tomba::render
