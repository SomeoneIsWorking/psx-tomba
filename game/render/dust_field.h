// game/render/dust_field.h — A0B FUN_801110BC: the node's dust, 513 white points of the camera lattice.
#pragma once

#include "state_producer.h"

struct Core;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba::render {

class DustField {
public:
  // Under the scene camera each lattice point (camera_lattice.h) is RTPSed and linked at bucket 0x100 as a white
  // 2x2 rect nearer than SZ 0x600, 1x1 past it. The guest keeps a point only for SX < 320; the cut is the draw window.
  static void draw(Core *c);

  static void registerOverrides(native::NativeOverrideCatalog &natives);
  // One call is the node's object; the render moves the camera by t and projects the points again.
  static void registerStateRenders(psx::present::StateProducerTable &renders, Core &core);
};

} // namespace tomba::render
