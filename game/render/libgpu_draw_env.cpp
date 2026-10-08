// game/render/libgpu_draw_env.cpp — libgpu SetDrawEnv (0x80081FB0).
//
// WHAT IT IS, IN GAME TERMS
// Once per buffer flip Tomba!2 hands the GPU a DRAWENV: "this frame draws into this rectangle of the
// framebuffer, with this origin, this texture page and texture window, and — if the environment asks
// for it — clear that rectangle to this colour first." SetDrawEnv is the COMPILER for that request:
// it turns the DRAWENV description into the six-or-nine-word GP0 command packet (a DR_ENV, living at
// DRAWENV+28) that the DMA sends to the GPU. Everything the player sees is drawn inside the clip
// rectangle this function encodes, and the background wipe between frames is the optional tail.
//
// HOW IT WAS IDENTIFIED (callers first, per CLAUDE.md — not from the neighbourhood)
//   * Ghidra headless (scratch/decomp/setdrawenv_81fb0.c, FUN_800815d0 + FUN_80081fb0). The caller
//     FUN_800815d0 is `SetDrawEnv(env+0x1c, env); env->dr_env.tag |= 0xffffff; dma_send(...);
//     memcpy(0x800a59b0, env, 0x5c); return env;` — that is libgpu PutDrawEnv verbatim, right down
//     to returning its own argument and caching the env as "the current one". The second caller
//     (gen_func at authenticated executable/overlay evidence, guest 0x800816A0) is the same shape but links an OT
//     pointer into the tag instead of the terminator: DrawOTagEnv. Both pass a0 = env+28, a1 = env,
//     which fixes a0 = DR_ENV destination and a1 = DRAWENV source.
//   * The five callees pin every word. They are already natively owned in
//     game/render/wide_re_gpu_putdrawenv.cpp and each emits one GP0 command:
//       0x80082240 -> 0xE3 clip top-left      0x800822D8 -> 0xE4 clip bottom-right
//       0x80082370 -> 0xE5 drawing offset     0x80082220 -> 0xE1 draw mode (tpage/dither/dfe)
//       0x8008238C -> 0xE2 texture window
//     Word 5 is a literal 0xE6000000 (mask-bit setting). The field offsets the calls read off a1
//     (0/2/4/6, 8/10, 12, 20, 22, 23, 24, 25/26/27) reproduce the SDK's DRAWENV layout exactly.
//   * NOT PlatformHle. The body has no spin loop and touches no hardware register: it only reads a
//     guest struct, calls five pure word-builders and stores words. See game/core/libapi_intr.cpp's
//     banner for the distinction — PlatformHle is for primitives that busy-wait on an IRQ this
//     runtime never raises.
//
// TRUE EXTENT: [0x80081FB0, 0x80082220), 0x270 bytes / 156 instructions. Established three ways, not
// assumed: `jr ra` sits at 0x80082218 with its delay slot `addiu sp,sp,40` at 0x8008221C (disas);
// 0x80082220 is a CALL TARGET of this very function, so it must be a function entry; and binary evidence's
// live-extent splitter reports 12834-12980 of authenticated executable/overlay evidence with the next guest-visible
// behavior in that shard being guest 0x80082504, i.e. no adjacent sibling was folded in. abi_extract's 4 "unreachable
// blocks" are the recorded binary evidence's duplicated `return;` tails inside this extent, not a folded sibling.
//
// THE TWO TRAPS IN THE TAIL
//  1. The clear primitive is chosen by ALIGNMENT, and the two choices use DIFFERENT COORDINATE
//     SPACES. If the clip rect's x and its (clamped) width are both multiples of 64, the packet gets
//     GP0(0x02) "fill VRAM rectangle" — the GPU's fast blind fill, which ignores the drawing offset
//     and clip, so it takes ABSOLUTE framebuffer coordinates. Otherwise it gets GP0(0x60) "monochrome
//     rectangle", an ordinary primitive that is drawn through the offset — so the coordinates must
//     have the drawing offset SUBTRACTED first. Swapping those two is a silent, alignment-dependent
//     misplacement of the background wipe.
//  2. The clamp against 0x800A59A4/0x800A59A6 (framebuffer limits, 1024 x 512 — verified in three
//     2 MB RAM dumps under scratch/bin/; the pair is written by GPU init, never by a static store in
//     authenticated executable/overlay evidence) compares the limit SIGNED and stores back the UNSIGNED read minus one.
//     It applies ONLY to the clear rect's w/h. The clip words themselves are clamped inside the 0xE3/0xE4 builders, and
//     the clear rect's x/y are NOT clamped at all.
//
// The 8-line stack RECT at sp+16..23 is real guest memory (SBS compares the guest stack), so it is
// mirrored through ClearRectScratch rather than held in C locals.
#include "libgpu_draw_env.h"
#include "core.h"
#include "core/overrides/guest_jal.h"
#include "core/overrides/native_override_catalog.h"
#include "game.h"
#include "guest_abi.h"

namespace {
// libgpu's framebuffer clip limits, read by this function and by the 0xE3/0xE4 word builders.
constexpr uint32_t kFrameBufferLimitX = 0x800A59A4u; // 1024
constexpr uint32_t kFrameBufferLimitY = 0x800A59A6u; // 512

// GP0 command bytes this function emits directly (the rest come back from the five builders).
constexpr uint32_t kGp0SetMaskBits = 0xE6000000u; // mask-bit setting, both bits clear
constexpr uint32_t kGp0FillVram = 0x02000000u;    // fill rectangle in VRAM — absolute coords
constexpr uint32_t kGp0FlatRect = 0x60000000u;    // monochrome rectangle — offset-relative

// GP0(0x02) can only fill on a 64-pixel grid, which is what selects between the two clear prims.
constexpr uint32_t kVramFillAlignMask = 63u;

// Words following the DR_ENV tag: the six state commands, plus the 3-word clear primitive.
constexpr uint32_t kPacketWordsPlain = 6;
constexpr uint32_t kPacketWordsWithClear = 9;

// Guest return addresses at this function's five jal sites (abi_extract --contract).
constexpr uint32_t kRaClipTopLeft = 0x80081FD8u;
constexpr uint32_t kRaClipBottomRight = 0x80082010u;
constexpr uint32_t kRaDrawOffset = 0x80082024u;
constexpr uint32_t kRaDrawMode = 0x8008203Cu;
constexpr uint32_t kRaTexWindow = 0x80082048u;

// Guest stack frame: 40 bytes, spilling s0/s1/ra (abi_extract --scaffold --guestabi).
constexpr GuestFrameSpill kSpills[3] = {
    {16, 24},
    {17, 28},
    {31 /*ra*/, 32},
};
} // namespace

uint32_t LibgpuDrawEnv::clampToFrameBuffer(Core *c, int32_t value, uint32_t limitAddr) {
  if (value < 0) {
    return 0;
  }
  const int32_t limitSigned = c->mem_r16s(limitAddr);
  if ((limitSigned - 1) < value) {
    return c->mem_r16(limitAddr) - 1u;
  }
  return (uint32_t)value;
}

// GUEST_ADDRESS: 80081FB0 authenticated executable/overlay evidence
// ORACLE: guest 0x80081FB0
// libgpu SetDrawEnv(DR_ENV *packet, DRAWENV *env) — compile the frame's drawing environment (clip
// rect, drawing origin, texture page/window, optional background clear) into the GP0 command packet.
void LibgpuDrawEnv::setDrawEnv(Core *c) {
  GuestFrame<40, 3> frame(c, kSpills);

  // s0/s1 stay LIVE in the register file across the five calls: the callees are guest functions that
  // spill their caller's callee-saved registers into their own frames, so a C++ local would leave
  // stale bytes on the guest stack (guest_abi.h's raison d'etre).
  GuestReg<16> envReg(c);
  GuestReg<17> packetReg(c);
  envReg = c->r[5];    // a1 = the DRAWENV being compiled
  packetReg = c->r[4]; // a0 = the DR_ENV packet to fill

  const DrawEnvFields env{c, c->r[16]};
  DrawEnvPacket packet{c, c->r[17]};

  // --- the six state commands, in packet order ---------------------------------------------------
  c->r[4] = (uint32_t)env.clipX();
  c->r[5] = (uint32_t)env.clipY();
  tomba::guest::dispatchJalToReturn(*c, 0x80082240u, kRaClipTopLeft);
  packet.setClipTopLeft(c->r[2]);

  // Bottom-right is inclusive: origin + size - 1, truncated back to s16 the way the guest does.
  c->r[4] = (uint32_t)(int32_t)(int16_t)(uint16_t)(env.clipWRaw() + env.clipXRaw() - 1u);
  c->r[5] = (uint32_t)(int32_t)(int16_t)(uint16_t)(env.clipYRaw() + env.clipHRaw() - 1u);
  tomba::guest::dispatchJalToReturn(*c, 0x800822D8u, kRaClipBottomRight);
  packet.setClipBottomRight(c->r[2]);

  c->r[4] = (uint32_t)env.offsetX();
  c->r[5] = (uint32_t)env.offsetY();
  tomba::guest::dispatchJalToReturn(*c, 0x80082370u, kRaDrawOffset);
  packet.setDrawOffset(c->r[2]);

  c->r[4] = env.drawOnDisplay();
  c->r[5] = env.dither();
  c->r[6] = env.texturePage();
  tomba::guest::dispatchJalToReturn(*c, 0x80082220u, kRaDrawMode);
  // The guest sets up the next call's argument in the delay slot BEFORE storing the mode word; kept
  // in that order so the store sequence matches the oracle statement for statement.
  c->r[4] = env.texWindowAddr();
  packet.setDrawMode(c->r[2]);
  tomba::guest::dispatchJalToReturn(*c, 0x8008238Cu, kRaTexWindow);
  packet.setTexWindow(c->r[2]);

  packet.setMaskBits(kGp0SetMaskBits);

  // --- the optional background clear --------------------------------------------------------------
  uint32_t packetWords = kPacketWordsPlain;
  if (env.clearsBackground()) {
    // The rect is assembled on the guest stack, first raw from the clip rect, then with its size
    // clamped into the framebuffer. x/y are deliberately left unclamped, exactly as the guest does.
    ClearRectScratch clear{c};
    clear.setClearX((uint16_t)env.clipXRaw());
    clear.setClearY((uint16_t)env.clipYRaw());
    clear.setClearW((uint16_t)env.clipWRaw());
    clear.setClearH((uint16_t)env.clipHRaw());
    clear.setClearW((uint16_t)clampToFrameBuffer(c, env.clipW(), kFrameBufferLimitX));
    clear.setClearH((uint16_t)clampToFrameBuffer(c, clear.clearHSigned(), kFrameBufferLimitY));

    const bool vramFillEligible =
        ((clear.clearX() & kVramFillAlignMask) == 0) && ((clear.clearW() & kVramFillAlignMask) == 0);
    if (!vramFillEligible) {
      // GP0(0x60) is an ordinary primitive: the GPU adds the drawing offset to its vertices, so the
      // absolute clip origin has to be pulled back into drawing-area space first.
      clear.setClearX((uint16_t)(clear.clearX() - env.offsetXRaw()));
      clear.setClearY((uint16_t)(clear.clearY() - env.offsetYRaw()));
      packet.setClearCommand(kGp0FlatRect | env.clearColor());
      packet.setClearTopLeft(clear.topLeftWord());
      packet.setClearSize(clear.sizeWord());
    } else {
      // GP0(0x02) writes VRAM directly, ignoring offset and clip — absolute coordinates, unchanged.
      packet.setClearCommand(kGp0FillVram | env.clearColor());
      packet.setClearTopLeft(clear.topLeftWord());
      packet.setClearSize(clear.sizeWord());
    }
    packetWords = kPacketWordsWithClear;
  }

  packet.setTagWordCount(packetWords);
  c->r[2] = packetWords; // v0 — the guest returns the word count it just stored
}

void LibgpuDrawEnv::registerOverrides(Game *) {
  tomba::native::declareOverride(0x80081FB0u, "LibgpuDrawEnv::setDrawEnv", &LibgpuDrawEnv::setDrawEnv);
}
