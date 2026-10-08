// Native assembly child-oscillator driver, guest entry 0x801316CC in the A00 overlay.
#include "substate_edge_native.h"
#include "assembly_node.h"
#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "guest_call.h"

namespace {
constexpr uint32_t kOscFrame = 32;
constexpr uint32_t kOscSpillSlot = 16;
constexpr uint32_t kOscSpillNode = 20;
constexpr uint32_t kOscSpillRa = 24;
constexpr uint32_t kRaAfterPartTick = 0x80131728u;
constexpr int32_t kFirstDrivenPart = 2;
constexpr int32_t kPartLimit = 4;
} // namespace

void SubstateEdgeLeaves::tickChildOscillators(Core *c) {
  // Guest frame, mirrored exactly: sp descends 32, r17/ra/r16 spill at +20/+24/+16.
  const uint32_t sp0 = c->r[29];
  c->r[29] = sp0 - kOscFrame;
  c->mem_w32(c->r[29] + kOscSpillNode, c->r[17]);
  c->r[17] = c->r[4]; // node stays in r17 for the whole body — see below
  c->mem_w32(c->r[29] + kOscSpillRa, c->r[31]);
  c->mem_w32(c->r[29] + kOscSpillSlot, c->r[16]);

  // The guest tick spills incoming r16/r17 into its own frame, so loop state is guest-visible:
  // counter and node pointer live in guest registers and are only named here.
  uint32_t &partIndex = c->r[16];
  const AssemblyNode node(c, c->r[17]);

  partIndex = kFirstDrivenPart;
  if (node.hasOscillatingParts()) {
    for (;;) {
      // slot = k*4 - 4, one lower when not in pair mode. The <<16 >>14 is sext16(k) * 4;
      // the shifts are the sign-extension, not a scale trick.
      const int32_t k = (int16_t)(uint16_t)partIndex;
      const int32_t slot = k * 4 - 4 - (node.oscillatorPairMode() ? 0 : 1);

      c->r[4] = node.addr();
      c->r[5] = (uint32_t)(int32_t)(int16_t)(uint16_t)slot;
      c->r[31] = kRaAfterPartTick;
      psx::cpu::dispatchGuestToReturn0(*c,
                                       0x80130D5Cu,
                                       psx::cpu::ExecutionBudget::currentTurn(*c),
                                       __func__); // the per-sub-part oscillator, runtime guest code

      // Re-read config after the call: the tick can clear pair mode, and when clear this
      // loop runs exactly once. Caching across the call would change behaviour.
      if (!node.oscillatorPairMode()) {
        break;
      }
      partIndex = (uint32_t)(int32_t)(int16_t)(uint16_t)(partIndex + 1);
      if ((int16_t)(uint16_t)partIndex >= kPartLimit) {
        break;
      }
    }
  }

  c->r[31] = c->mem_r32(c->r[29] + kOscSpillRa);
  c->r[17] = c->mem_r32(c->r[29] + kOscSpillNode);
  c->r[16] = c->mem_r32(c->r[29] + kOscSpillSlot);
  c->r[29] = sp0;
}

void SubstateEdgeLeaves::registerOverrides(Game *) {
  // Of 23 authenticated MODE images only A00 holds a function at 0x801316CC.
  tomba::native::declareOverlayOverride(
      "A00", 0x801316CCu, "SubstateEdgeLeaves::tickChildOscillators", &SubstateEdgeLeaves::tickChildOscillators);
}
