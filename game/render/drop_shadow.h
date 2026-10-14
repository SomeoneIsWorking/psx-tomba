// game/render/drop_shadow.h — FUN_8002AB5C, the drop shadow drawn under an actor.
//
// The shadow is a type-0x20 object whose draw function is 0x8002AB5C (SOP intro, sop_overlay_shadow.cpp). Its
// node carries a fade (+0x4E, 0..0x80), a size ramp (+0x50, 0..0x100), the authored x and z extents (+0x40,
// +0x42), its world position (+0x48) and angles (+0x54). The draw scales the shadow mesh 0x8009FAE8 by the
// ramp, composes it with the camera like FUN_800318A0 and writes it through the packed-mesh writer, faded
// toward black by IR0.
#pragma once

#include <cstdint>

class Core;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba::render {

class DropShadow {
public:
  static constexpr std::uint32_t kDraw = 0x8002AB5Cu;
  static constexpr std::uint32_t kMesh = 0x8009FAE8u;
  // The mesh's three column scale bytes; the draw rewrites x and z, y stays as loaded.
  static constexpr std::uint32_t kScale = 0x800A2014u;

  // FUN_8002AB5C with the node in a0.
  static void draw(Core &core);
  // Each shadow is an object of its own: its render record's current life.
  static void registerOverrides(native::NativeOverrideCatalog &natives);
};

} // namespace tomba::render
