// game/render/gradient_backdrop.h — A0E's boss arena sky FUN_80114178: four gouraud bands behind the scene.
#pragma once

#include "state_producer.h"

struct Core;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba::render {

class GradientBackdrop {
public:
  // FUN_80114178: the bands across the draw window at the background bucket, their draw mode, then FUN_80111E14.
  static void emit(Core *c);

  static void registerOverrides(native::NativeOverrideCatalog &natives);
  // One call is one object; the render moves the bands with the camera pitch.
  static void registerStateRenders(psx::present::StateProducerTable &renders);
};

} // namespace tomba::render
