// game/render/libgpu_draw_env.h — libgpu SetDrawEnv (0x80081FB0): the DRAWENV -> DR_ENV compiler.
//
// See libgpu_draw_env.cpp for the identification evidence. This header holds only the typed LENSES
// over the two guest structs the function reads and writes, plus the named layout constants, so the
// body reads as "compile a drawing environment into a GPU command packet" rather than as a run of
// mem_r16(base + 0x16) calls.
//
// The write accessors are deliberately ONE-LINERS: tools/dynamic differential evidence harvests a lens setter's
// mem_wN widths by regex so `packet.setDrawMode(v)` counts as exactly the store it performs. A setter
// that grew a second statement or a nested brace would silently stop counting.
#pragma once
#include "core.h"
#include <cstdint>

class Game;

// ------------------------------------------------------------------------------------------------
// DRAWENV — the SDK struct the GAME fills in and hands to PutDrawEnv once per buffer flip. It is a
// description of the drawing environment, not a GPU packet: where on the framebuffer this frame may
// draw, where its origin sits, which texture page/window is bound, and whether the GPU should clear
// the area to a flat colour before anything is drawn.
namespace drawenv {
constexpr uint32_t kClipX = 0;        // s16 — clip RECT origin, absolute framebuffer coords
constexpr uint32_t kClipY = 2;        // s16
constexpr uint32_t kClipW = 4;        // u16 — clip RECT size (BR = x+w-1, y+h-1)
constexpr uint32_t kClipH = 6;        // u16
constexpr uint32_t kOffsetX = 8;      // s16 — drawing origin, added to every primitive's vertices
constexpr uint32_t kOffsetY = 10;     // s16
constexpr uint32_t kTexWindow = 12;   // RECT (8 bytes) — texture-window mask/offset, read by the
                                      //       GP0(0xE2) builder at 0x8008238C via its ADDRESS
constexpr uint32_t kTexturePage = 20; // u16 — packed tpage bits
constexpr uint32_t kDither = 22;      // u8  — DRAWENV.dtd:  dither on -> GP0(0xE1) bit 9
constexpr uint32_t kDrawOnDisp = 23;  // u8  — DRAWENV.dfe:  allow drawing into the displayed area
constexpr uint32_t kClearFlag = 24;   // u8  — DRAWENV.isbg: clear the clip rect before drawing
constexpr uint32_t kClearRed = 25;    // u8  — DRAWENV.r0/g0/b0: the clear colour
constexpr uint32_t kClearGreen = 26;  // u8
constexpr uint32_t kClearBlue = 27;   // u8
} // namespace drawenv

// Read-only lens over a guest DRAWENV. `Raw` accessors are the zero-extended (lhu) reads; the plain
// ones are sign-extended (lh) — the distinction is load-bearing, the clip size is compared SIGNED
// against the framebuffer limit but stored back UNSIGNED.
struct DrawEnvFields {
  Core *mCore;
  uint32_t mBase;

  int32_t clipX() const {
    return mCore->mem_r16s(mBase + drawenv::kClipX);
  }
  int32_t clipY() const {
    return mCore->mem_r16s(mBase + drawenv::kClipY);
  }
  int32_t clipW() const {
    return mCore->mem_r16s(mBase + drawenv::kClipW);
  }
  uint32_t clipXRaw() const {
    return mCore->mem_r16(mBase + drawenv::kClipX);
  }
  uint32_t clipYRaw() const {
    return mCore->mem_r16(mBase + drawenv::kClipY);
  }
  uint32_t clipWRaw() const {
    return mCore->mem_r16(mBase + drawenv::kClipW);
  }
  uint32_t clipHRaw() const {
    return mCore->mem_r16(mBase + drawenv::kClipH);
  }
  int32_t offsetX() const {
    return mCore->mem_r16s(mBase + drawenv::kOffsetX);
  }
  int32_t offsetY() const {
    return mCore->mem_r16s(mBase + drawenv::kOffsetY);
  }
  uint32_t offsetXRaw() const {
    return mCore->mem_r16(mBase + drawenv::kOffsetX);
  }
  uint32_t offsetYRaw() const {
    return mCore->mem_r16(mBase + drawenv::kOffsetY);
  }

  uint32_t texWindowAddr() const {
    return mBase + drawenv::kTexWindow;
  }
  uint32_t texturePage() const {
    return mCore->mem_r16(mBase + drawenv::kTexturePage);
  }
  uint32_t dither() const {
    return mCore->mem_r8(mBase + drawenv::kDither);
  }
  uint32_t drawOnDisplay() const {
    return mCore->mem_r8(mBase + drawenv::kDrawOnDisp);
  }
  bool clearsBackground() const {
    return mCore->mem_r8(mBase + drawenv::kClearFlag) != 0;
  }

  // The GP0 colour field, 0x00BBGGRR — ORed straight into the clear primitive's command word.
  uint32_t clearColor() const {
    return (mCore->mem_r8(mBase + drawenv::kClearBlue) << 16) | (mCore->mem_r8(mBase + drawenv::kClearGreen) << 8) |
           mCore->mem_r8(mBase + drawenv::kClearRed);
  }
};

// ------------------------------------------------------------------------------------------------
// DR_ENV — the GPU command packet SetDrawEnv produces, living at DRAWENV+28. Word 0 is the OT tag
// (its top byte is the word count that follows); words 1.. are raw GP0 commands the DMA sends
// verbatim. Six words always, nine when the environment asks for a background clear.
namespace drenv {
constexpr uint32_t kTagWordCount = 3;    // u8 — top byte of the tag word: words following the tag
constexpr uint32_t kClipTopLeft = 4;     // GP0(0xE3)
constexpr uint32_t kClipBottomRight = 8; // GP0(0xE4)
constexpr uint32_t kDrawOffset = 12;     // GP0(0xE5)
constexpr uint32_t kDrawMode = 16;       // GP0(0xE1)
constexpr uint32_t kTexWindow = 20;      // GP0(0xE2)
constexpr uint32_t kMaskBits = 24;       // GP0(0xE6)
constexpr uint32_t kClearCommand = 28;   // GP0(0x02) or GP0(0x60) + colour
constexpr uint32_t kClearTopLeft = 32;   // packed {x, y}
constexpr uint32_t kClearSize = 36;      // packed {w, h}
} // namespace drenv

// Write lens over the DR_ENV packet. One store per setter, on one line — see the header note.
struct DrawEnvPacket {
  Core *mCore;
  uint32_t mBase;

  void setClipTopLeft(uint32_t v) {
    mCore->mem_w32(mBase + drenv::kClipTopLeft, v);
  }
  void setClipBottomRight(uint32_t v) {
    mCore->mem_w32(mBase + drenv::kClipBottomRight, v);
  }
  void setDrawOffset(uint32_t v) {
    mCore->mem_w32(mBase + drenv::kDrawOffset, v);
  }
  void setDrawMode(uint32_t v) {
    mCore->mem_w32(mBase + drenv::kDrawMode, v);
  }
  void setTexWindow(uint32_t v) {
    mCore->mem_w32(mBase + drenv::kTexWindow, v);
  }
  void setMaskBits(uint32_t v) {
    mCore->mem_w32(mBase + drenv::kMaskBits, v);
  }
  void setClearCommand(uint32_t v) {
    mCore->mem_w32(mBase + drenv::kClearCommand, v);
  }
  void setClearTopLeft(uint32_t v) {
    mCore->mem_w32(mBase + drenv::kClearTopLeft, v);
  }
  void setClearSize(uint32_t v) {
    mCore->mem_w32(mBase + drenv::kClearSize, v);
  }
  void setTagWordCount(uint32_t v) {
    mCore->mem_w8(mBase + drenv::kTagWordCount, (uint8_t)v);
  }
};

// ------------------------------------------------------------------------------------------------
// The clear rectangle SetDrawEnv assembles on its OWN guest stack frame (sp+16..sp+23) before
// copying it into the packet as two 32-bit words. It is a real guest-memory RECT, not a C local:
// SBS byte-compares the guest stack, so these stores must land where the guest body puts them.
namespace clearrect {
constexpr uint32_t kX = 16;
constexpr uint32_t kY = 18;
constexpr uint32_t kW = 20;
constexpr uint32_t kH = 22;
} // namespace clearrect

struct ClearRectScratch {
  Core *mCore;

  uint32_t base() const {
    return mCore->r[29];
  }

  void setClearX(uint16_t v) {
    mCore->mem_w16(base() + clearrect::kX, v);
  }
  void setClearY(uint16_t v) {
    mCore->mem_w16(base() + clearrect::kY, v);
  }
  void setClearW(uint16_t v) {
    mCore->mem_w16(base() + clearrect::kW, v);
  }
  void setClearH(uint16_t v) {
    mCore->mem_w16(base() + clearrect::kH, v);
  }

  uint32_t clearX() const {
    return mCore->mem_r16(base() + clearrect::kX);
  }
  uint32_t clearY() const {
    return mCore->mem_r16(base() + clearrect::kY);
  }
  uint32_t clearW() const {
    return mCore->mem_r16(base() + clearrect::kW);
  }
  int32_t clearHSigned() const {
    return mCore->mem_r16s(base() + clearrect::kH);
  }

  // The packet takes the rect as two words, exactly as the two u16 pairs sit in memory.
  uint32_t topLeftWord() const {
    return mCore->mem_r32(base() + clearrect::kX);
  }
  uint32_t sizeWord() const {
    return mCore->mem_r32(base() + clearrect::kW);
  }
};

// ------------------------------------------------------------------------------------------------
class LibgpuDrawEnv {
public:
  // FUN_80081FB0 — libgpu SetDrawEnv(DR_ENV *packet, DRAWENV *env). Compiles the drawing
  // environment into the GP0 command packet PutDrawEnv/DrawOTagEnv hand to the GPU DMA.
  static void setDrawEnv(Core *c);

  static void registerOverrides(Game *game);

private:
  // libgpu's "keep the clip rect inside the framebuffer" clamp, shared with the GP0(0xE3)/(0xE4)
  // builders. Reads the limit SIGNED for the comparison and UNSIGNED for the clamped result — that
  // asymmetry is the guest's, not a slip.
  static uint32_t clampToFrameBuffer(Core *c, int32_t value, uint32_t limitAddr);
};
