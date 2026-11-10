// game/render/backdrop_plane.h — A0E's boss arena backdrop FUN_80110CA4: the waterfall plane, its reflection and
// the haze band along their seam.
#pragma once

struct Core;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba::render {

class BackdropPlane {
public:
  // FUN_80110CA4(node=a0), the node's render callback (node+0x18). Under the node's view (rotation node+0x48,
  // position node+0x2C, scene camera 0x1F8000F8) it draws a 10x7 grid of 1200x2400 GT4s at model z 0, mirrored
  // in v about y 0, then 10 additive GT4s from z 0 to 1200 along y 0; then FUN_801104D0(node).
  static void draw(Core *c);

  static void registerOverrides(native::NativeOverrideCatalog &natives);
};

} // namespace tomba::render
