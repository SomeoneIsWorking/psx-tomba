// class OptionsPage — FUN_8007FC24, the OPTIONS page backdrop emitter.
//
// The five OPTIONS page builders (FUN_8007F104/F250/F498/F73C/F8F8, selected by FUN_8007B45C on
// task-sm[0x50]) serve both the DEMO/title front-end and the in-game START page; all but "Screen
// adjust" call FUN_8007FC24 for the full-screen dark-blue gradient.
#pragma once
class Core;

class OptionsPage {
public:
  // Registers FUN_8007FC24. Idempotent.
  static void install();

  // FUN_8007FC24: one 36-byte POLY_G4 packet (the 320x240 gradient) from the guest packet pool,
  // linked at ordering-table bucket 1.
  static void pushBackdrop(Core *c);
};
