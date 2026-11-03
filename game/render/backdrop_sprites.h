// game/render/backdrop_sprites.h — A0E's boss arena FUN_80111E14: eight world-anchored sprites at the background
// bucket, drawn after the sky bands.
#pragma once

#include "state_producer.h"

struct Core;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba::render {

class BackdropSprites {
public:
  // FUN_80111E14: seeds the eight anchors once, then under the scene camera RTPSes each and stamps the sprite records
  // 0x8011B8EC through ScaledSprites at bucket 0x7FE, scaled by MAC0.
  static void draw(Core *c);

  static void registerOverrides(native::NativeOverrideCatalog &natives);
  // One call is one object; the render projects the anchors again under the camera between the two calls.
  static void registerStateRenders(psx::present::StateProducerTable &renders, Core &core);
};

} // namespace tomba::render
