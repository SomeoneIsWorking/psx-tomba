// The card browser's check page, CRD 0x8018DC2C: "Checking MEMORY CARD..." until both ports' checks settle.
#pragma once

class Core;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba::scene {

class CardCheck {
public:
  // CRD 0x8018DC2C(browser): retail holds the page 11 frames before it waits on the ports; this waits only on them.
  static void step(Core *core);

  static void registerOverrides(native::NativeOverrideCatalog &natives);
};

} // namespace tomba::scene
