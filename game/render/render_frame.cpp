// render_frame.cpp — Render::frame/frameX, the per-frame render orchestrators (guest bodies).
//
// RE (MAIN.EXE, tools/disas.py):
//   0x8003f9a8 render orchestrator: jal 0x8004fd30, 0x80025d98, 0x8003bf00, 0x8003eec0, 0x8003b588,
//     0x8003bb50, 0x8003bcf4, 0x8003d0bc(a0=0x800f2418), 0x8003f024, 0x8003df04, 0x8003c048.
//   0x8003fa44 (transition twin): jal 0x8004fd30, 0x80025d98, 0x8003bf00, 0x8003eec0, 0x8003b588,
//     0x8003bb50, 0x8003bcf4, 0x8003c048, 0x8003f024.
// Guest callers besides Engine::fieldFrame/fieldFrameX: GAME 0x8010737C, 0x80107638, 0x801078CC, 0x801074D0
// (the guest submode frames) and DEMO 0x80106EBC (the attract demo's frame).

#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "entry/session.h"
#include "guest_call.h"
#include "native_dispatch.h"
#include "render.h" // class Render — methods live here

namespace tomba {
namespace {

void ovFrame(Core *c) {
  rend(c)->frame();
}

void ovFrameX(Core *c) {
  rend(c)->frameX();
}

} // namespace

// 0x8003f9a8 — per-frame render orchestrator; runs as the guest body.
void Render::frame() {
  Core *c = mCore;
  sessionOf(c).drawReplay.beforeDraw(*c, DrawSection::Variant::Field);
  psx::cpu::callOriginalToReturn(*c, kFieldDraw, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
}

// 0x8003fa44 — mid-transition render orchestrator twin (reduced pass set).
void Render::frameX() {
  Core *c = mCore;
  sessionOf(c).drawReplay.beforeDraw(*c, DrawSection::Variant::Room);
  psx::cpu::callOriginalToReturn(*c, kRoomDraw, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
}

void Render::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declare(kFieldDraw, "Render::frame", ovFrame);
  natives.declare(kRoomDraw, "Render::frameX", ovFrameX);
}

} // namespace tomba
