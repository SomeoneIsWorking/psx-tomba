// game/ai/zoned_attacker_abi.h — the guest ABI vocabulary the ActorZonedAttacker bodies share:
// guest register slots, the leaf and global addresses they dispatch to, the two leaf-call shorthands,
// and the guest-stack frame contracts the override trampolines must mirror.
//
// Every leaf reached through these addresses is an un-owned PSX body called through
// psx::cpu::dispatchGuestToReturn0; two of them (0x800777FC, 0x800518FC) also have native owners
// elsewhere and are still reached here so the whole cluster dispatches uniformly.
#pragma once

#include "core.h"
#include "guest_abi.h"
#include "guest_call.h"

#include <cstdint>

namespace tomba::ai::zoned {

enum GuestReg : int { R_V0 = 2, R_A0 = 4, R_A1 = 5, R_A2 = 6, R_A3 = 7 };

// Leaves dispatched to by address.
inline constexpr uint32_t kZoneClassify = 0x80145C78u;
inline constexpr uint32_t kRngRead = 0x8009A450u;
inline constexpr uint32_t kDist2D = 0x800781E0u;
inline constexpr uint32_t kDist3D = 0x80078240u;
inline constexpr uint32_t kSetAnimStateCue = 0x801402B8u;
inline constexpr uint32_t kMotionAnimStep = 0x801406E4u;
inline constexpr uint32_t kAnimationStep = 0x80076D68u;
inline constexpr uint32_t kInstallTypeTable = 0x800519E0u;
inline constexpr uint32_t kGridResolve = 0x8004766Cu;
inline constexpr uint32_t kLeaf_80049674 = 0x80049674u;
inline constexpr uint32_t kLeaf_800782B0 = 0x800782B0u;
inline constexpr uint32_t kLeaf_80142788 = 0x80142788u;
inline constexpr uint32_t kLeaf_801425F0 = 0x801425F0u;
inline constexpr uint32_t kLeaf_80141AC4 = 0x80141AC4u;
inline constexpr uint32_t kLeaf_801422B4 = 0x801422B4u;
inline constexpr uint32_t kLeaf_8014213C = 0x8014213Cu;
inline constexpr uint32_t kLeaf_80141C20 = 0x80141C20u;
inline constexpr uint32_t kLeaf_80140AF4 = 0x80140AF4u;
inline constexpr uint32_t kLeaf_8014243C = 0x8014243Cu;
inline constexpr uint32_t kLeaf_801436C4 = 0x801436C4u;
inline constexpr uint32_t kLeaf_801431C4 = 0x801431C4u;
inline constexpr uint32_t kLeaf_801408AC = 0x801408ACu;
inline constexpr uint32_t kLeaf_8014103C = 0x8014103Cu;
inline constexpr uint32_t kLeaf_80141438 = 0x80141438u;
inline constexpr uint32_t kLeaf_8014181C = 0x8014181Cu;
inline constexpr uint32_t kLeaf_80142A94 = 0x80142A94u;
inline constexpr uint32_t kLeaf_80142CF4 = 0x80142CF4u;
inline constexpr uint32_t kLeaf_80026100 = 0x80026100u;
inline constexpr uint32_t kSfxTrigger = 0x80074590u;
inline constexpr uint32_t kPaletteSideEffect = 0x80077E20u;
inline constexpr uint32_t kLeaf_80080750 = 0x80080750u;
inline constexpr uint32_t kLeaf_801280E8 = 0x801280E8u;
inline constexpr uint32_t kLeaf_80077768 = 0x80077768u;
inline constexpr uint32_t kCullWrapperFlag2 = 0x800777FCu;
inline constexpr uint32_t kObjMatrixCompose = 0x800518FCu;
inline constexpr uint32_t kLeaf_800495DC = 0x800495DCu;
inline constexpr uint32_t kLeaf_800315D4 = 0x800315D4u;
inline constexpr uint32_t kGateCheck = 0x8014047Cu;
inline constexpr uint32_t kPickAttackByRange = 0x801409C0u;

// Globals the bodies read and write.
inline constexpr uint32_t kGateTimer = 0x800E7EAAu;       // u8
inline constexpr uint32_t kGateScratch = 0x800E7EACu;     // address only, passed as a pointer argument
inline constexpr uint32_t kGlobal_800ED098 = 0x800ED098u; // i16
inline constexpr uint32_t kTableBase = 0x800ECFB0u;       // u32, installed per type
inline constexpr uint32_t kNodeFieldBase = 0x8014BE14u;   // address only, passed as a pointer argument
inline constexpr uint32_t kCopyScratch = 0x800ECFB4u;     // u32, copied raw into node+0x3C
inline constexpr uint32_t kS_1F8001A0 = 0x1F8001A0u;      // u16
inline constexpr uint32_t kS_1F8001A2 = 0x1F8001A2u;      // u16
inline constexpr uint32_t kS_1F800160 = 0x1F800160u;      // i16
inline constexpr uint32_t kS_1F800162 = 0x1F800162u;      // i16
inline constexpr uint32_t kS_1F800164 = 0x1F800164u;      // i16
inline constexpr uint32_t kS_800E7FFE = 0x800E7FFEu;      // u16
inline constexpr uint32_t kTable_8014BEE4 = 0x8014BEE4u;  // u8[16]
inline constexpr uint32_t kTable_8014BED4 = 0x8014BED4u;  // u8[16]
inline constexpr uint32_t kTable_8014BEF4 = 0x8014BEF4u;  // u8[16]
inline constexpr uint32_t kS_1F800137 = 0x1F800137u;      // u8
inline constexpr uint32_t kS_800BF89C = 0x800BF89Cu;      // u8
inline constexpr uint32_t kS_800BF809 = 0x800BF809u;      // u8
inline constexpr uint32_t kCountdown = 0x8014BF5Eu;       // u8, shared with the caller's own tail
inline constexpr uint32_t kS_800E7E80 = 0x800E7E80u;      // u8

inline void call2(Core *c, uint32_t node, uint32_t addr, uint32_t a1, uint32_t a2) {
  c->r[R_A0] = node;
  c->r[R_A1] = a1;
  c->r[R_A2] = a2;
  psx::cpu::dispatchGuestToReturn0(*c, addr, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
}

inline void call1(Core *c, uint32_t node, uint32_t addr) {
  c->r[R_A0] = node;
  psx::cpu::dispatchGuestToReturn0(*c, addr, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
}

// Guest-stack frame contracts for the trampolines that allocate a frame. Each must mirror its
// substrate body's allocation and callee-save spills, or the guest-stack bytes diverge. Tables come
// from `python3 tools/binary ABI evidence <addr> --scaffold --guestabi`, in program order.
inline constexpr GuestFrameSpill kSpills_80140544[4] = {{16, 16}, {17, 20}, {18, 24}, {31, 28}};           // frame=32
inline constexpr GuestFrameSpill kSpills_8014047C[2] = {{16, 16}, {31, 20}};                               // frame=24
inline constexpr GuestFrameSpill kSpills_80144928[4] = {{16, 16}, {17, 20}, {18, 24}, {31, 28}};           // frame=32
inline constexpr GuestFrameSpill kSpills_801409C0[5] = {{18, 24}, {16, 16}, {31, 32}, {19, 28}, {17, 20}}; // frame=40
inline constexpr GuestFrameSpill kSpills_80143A00[5] = {{16, 16}, {31, 32}, {19, 28}, {18, 24}, {17, 20}}; // frame=40
inline constexpr GuestFrameSpill kSpills_80144B50[4] = {{16, 32}, {31, 44}, {18, 40}, {17, 36}};           // frame=48

} // namespace tomba::ai::zoned