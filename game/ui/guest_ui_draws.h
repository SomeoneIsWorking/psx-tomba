// class GuestUiDraws — the guest functions that draw only screen-space UI and stay guest bodies: crop
// widescreen shows what they emit at its 4:3 size and place.
//
//   FUN_80025744  HUD life gauge (from the HUD draw FUN_80025D98)
//   FUN_80025934  HUD piece tally (from FUN_80025D98)
//   FUN_80025B78  HUD item strip, its DR_AREA clip included (from FUN_80025D98)
//   FUN_8007EAE4  pause menu main page: item rows, feather cursor, frame sprites (from GAME FUN_801084F8)
#pragma once
class Core;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba {

class GuestUiDraws {
public:
  static void install(native::NativeOverrideCatalog &natives);
};

} // namespace tomba
