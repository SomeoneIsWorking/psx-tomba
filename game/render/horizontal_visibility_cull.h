// game/render/horizontal_visibility_cull.h — the title's HORIZONTAL VISIBILITY CULL, as recovered
// from the authenticated images, owned once.
//
// WHY THIS FILE EXISTS. A widened picture is only as wide as the narrowest bound that decides what may
// appear in the new margin, and the workspace discriminator WIDENED BUT ... MARGIN IS NOT COVERAGE is
// exactly that failure. Tomba! 2 had no survey of its own, so nobody could say how many bounds there
// were, which of them were literals, and which of them the port had actually widened. This is that
// survey's result, as code.
//
// THE RECOVERED PREDICATE. A scan of all 29 authenticated code images
// (3,823,581 B, 955,895 words; 520,754 words — 54.5% — attributed to a walked function body by
// control-flow reachability from a prologue) for frame-derived constants, and grouped the hits by the
// body containing them. 125 body instances, in 96 structurally distinct bodies, make a compare
// against a 4:3-derived value. Of those:
//
//   70 structurally distinct bodies (99 instances) are CULLING OWNERS. Each makes one screen-edge
//   compare per primitive CORNER, UNSIGNED, OR-ed, with the X gate and the Y gate AND-ed. The
//   representative is guest 0x8003B320 (MAIN.EXE, the quad submitter), read instruction by
//   instruction at 0x8003B438:
//         lhu   $v0, 8($a3)        ; corner 0 SX
//         sltiu $v0, $v0, 0x140    ; SX < 320, UNSIGNED
//         bnez  $v0, KEEP          ; any corner inside -> keep
//         ... corners 1, 2, 3 the same, each branching to KEEP
//         ... then the same four against 0xF0 for SY, each branching to KEEP
//   and the drop target is taken only when BOTH gates fail. Some owners are the 3-corner (triangle)
//   form, some the 16.16 form applied to the raw 32-bit GTE SX1/SX2/SX3 registers — e.g. A0L
//   0x80112E70, `lui $t9,0xf0; sltu $t8,$v1,$t9; ... lui $t9,0x140; sll $v1,$v1,0x10; sltu` — which is
//   the SAME predicate in a different representation, not a different owner.
//
//   20 bodies are LAYOUT, not culling: they compare a SIGNED value (`slti`) against 320/240, so a
//   negative coordinate PASSES, or they compare against the projection centre 160/120. A bound a
//   negative coordinate passes is a position limit, not a visibility test.
//
//   6 bodies are NEITHER: a single edge compare with no per-corner structure.
//
// THE THREE PROPERTIES THAT MATTER, and all three are recovered rather than chosen:
//
//   1. THE COMPARE IS UNSIGNED. `sltiu`/`sltu` against 320 means a projected coordinate that wrapped
//      negative reads as 0x8000..0xFFFF and FAILS the test. That is why the guest drops geometry to
//      the LEFT of and ABOVE the screen, and it is not a bug to be tidied away: reproducing it is
//      what makes the widened cull identical to retail inside the 4:3 region. A signed compare here
//      would be a different owner with a different picture.
//
//   2. THE GATES ARE OR-OVER-CORNERS, THEN AND-OF-AXES. Keep iff (some corner X inside) AND (some
//      corner Y inside). A quad straddling the right edge survives on its left corners; a quad
//      entirely off both edges does not. Reproducing the gates as a single bounding-box test would
//      change which primitives survive at the boundary.
//
//   3. NO OWNER DERIVES ITS BOUND. A second scan looked for reads of the DRAWENV the guest itself
//      publishes (PutDrawEnv copies 92 bytes to the "current env" cache at 0x800A59B0, and its +4/+6
//      are the clip width/height). Across all 29 images it found ZERO definite reads: every
//      screen-edge bound in Tomba! 2 is a literal 320 or 240. So there is no derived bound that
//      already widens, and a port that widens must supply the widened value itself. This owner takes
//      the draw window as an INPUT for that reason, and that is the whole design.
//
// WHY THE VERTICAL BOUND IS A CONSTANT AND THE HORIZONTAL ONE IS NOT. The projection publication is
// guest 0x800509B4, read from the binary: it calls the libgte InitGeom at 0x80083FF8, then
// SetGeomOffset(160, 120) at 0x800846D0 with a0=0xA0 and a1=0x78 in its delay slot, then
// SetGeomScreen(350) at 0x800846F0 with a0=0x15E, caching H=350 at 0x801003F8. The framework's
// projection is sx = OFX + ir0*(H/Sz) (external/psxport/runtime/psx/native_projection.cpp), so the
// visible horizontal half-extent at depth Sz is OFX*Sz/H and the horizontal field of view is the
// ratio OFX/H. Widening OFX at unchanged H grows the frustum by exactly the canvas ratio and leaves
// the vertical field of view and the central scale alone; OFY and H are untouched by the widening, so
// the vertical extent is 2*120 = 240 whatever the canvas width is. The horizontal extent is
// 2*OFX and therefore MOVES. One of the two bounds is a function of the canvas and the other is not,
// and this class is built on that fact rather than on a pair of literals that happen to match today.
//
// 4:3 IDENTITY. At the window [0, 320) this class's decision is bit-identical to the guest's for every
// input, which is the contract the tests pin. It is not "close to" retail; it is the same predicate
// with the same domain on the same numbers, and a widened run differs from 4:3 only in the bound.
//
// PURE, DELIBERATELY. No Core, no Render, no globals, no environment reads: the bound arrives as an
// argument so the class is decided by pure arithmetic and tested hermetically, the way
// page_gradient.h is. The Core-taking factory lives in the .cpp and is the only thing that knows the
// framework's wide-engine width.
#pragma once

#include <cstdint>

namespace tomba2::horizontal_cull {

// The retail 4:3 frame the guest's literals are DERIVED from. These two constants name the identity
// this class must reproduce; they are never a widened value, and no member of this namespace is a
// substitute for the runtime draw width.
inline constexpr int kGuestFrameWidth = 320;
inline constexpr int kGuestFrameHeight = 240;

// Corner counts the scan found. 3 is the triangle submitter's form, 4 the quad's; both are the same
// predicate over a different number of corners, which is why count is data and not a shape.
inline constexpr int kTriangleCorners = 3;
inline constexpr int kQuadCorners = 4;

// Which representation a corner coordinate arrives in.
//
//   Packed16    the guest's `lhu` of a packed SXY word: one 16-bit X and one 16-bit Y per corner.
//   Fixed16_16  the raw 32-bit GTE SX1/SX2/SX3 registers, which are already 16.16 fixed point. This
//               is the A0L 0x80112E70 form (`lui $t9,0x140; sll $v1,$v1,0x10; sltu $t8,$v1,$t9`) and it
//               is the same predicate: shifting both sides by 16 is what the guest does by hand.
enum class Domain {
  Packed16,
  Fixed16_16,
};

// The one axis test, recovered: `(uint32_t)coordinate < bound`, UNSIGNED.
//
// Unsigned is the whole content of this function. A coordinate the GTE saturated negative reads as
// 0x8000..0xFFFF (Packed16) or 0xFFFF8000..0xFFFFFFFF (Fixed16_16) and therefore FAILS, which is the
// guest's left/top drop. A signed compare here would keep exactly the geometry retail drops.
constexpr bool insideAxis(std::uint32_t coordinate, int bound, Domain domain) {
  const std::uint32_t limit =
      domain == Domain::Fixed16_16 ? static_cast<std::uint32_t>(bound) << 16 : static_cast<std::uint32_t>(bound);
  return coordinate < limit;
}

// The horizontal visibility cull for one draw window [left, right).
//
// A corner is inside iff (x - left), in the domain's width, is unsigned-below (right - left). With
// left = 0 that is the guest's own compare; the record path's canvas passes left = -M so a corner in
// the left margin passes and a wrapped one still fails.
class Visibility {
public:
  Visibility(int left, int right) : mLeft(left), mRight(right) {}

  int left() const {
    return mLeft;
  }
  int right() const {
    return mRight;
  }

  // The X gate: KEEP iff at least one of the `count` corners is inside [left(), right()).
  bool keepsX(const std::uint32_t *corners, int count, Domain domain) const {
    for (int index = 0; index < count; ++index) {
      if (insideAxis(shifted(corners[index], domain), mRight - mLeft, domain)) {
        return true;
      }
    }
    return false;
  }

  // The Y gate: KEEP iff at least one of the `count` corners is inside [0, kGuestFrameHeight).
  // Widening OFX leaves OFY and H alone, so the vertical extent is fixed.
  bool keepsY(const std::uint32_t *corners, int count, Domain domain) const {
    for (int index = 0; index < count; ++index) {
      if (insideAxis(corners[index], kGuestFrameHeight, domain)) {
        return true;
      }
    }
    return false;
  }

  // The recovered predicate: the X gate OR-ed over corners AND the Y gate OR-ed over corners.
  bool keeps(const std::uint32_t *xs, const std::uint32_t *ys, int count, Domain domain) const {
    return keepsX(xs, count, domain) && keepsY(ys, count, domain);
  }

  // True when this instance is the retail 4:3 decision.
  bool isGuestFrame() const {
    return mLeft == 0 && mRight == kGuestFrameWidth;
  }

private:
  std::uint32_t shifted(std::uint32_t coordinate, Domain domain) const {
    if (domain == Domain::Fixed16_16) {
      return coordinate - (static_cast<std::uint32_t>(mLeft) << 16);
    }
    return (coordinate - static_cast<std::uint32_t>(mLeft)) & 0xFFFFu;
  }

  int mLeft;
  int mRight;
};

// The cull for the window THIS port is drawing into, read once from the module that owns it
// (game/render/wide_window.h). The only impure step in the whole owner, and it is here so the class
// above stays testable without a Core. Declared here rather than in the .cpp so a consumer including
// only this header still gets the factory.
} // namespace tomba2::horizontal_cull

// The forward declaration is at GLOBAL scope, and the parameter is named `::Core` for the same reason:
// `struct Core;` written inside `tomba2::horizontal_cull` DECLARES a new type in that namespace rather
// than naming the framework's, so the factory would take a `tomba2::horizontal_cull::Core *` that no
// caller possesses and every call site would fail to convert. That is not hypothetical -- it is what
// the first version wrote, and the build rejected it with exactly that error.
struct Core;

namespace tomba2::horizontal_cull {

Visibility forDrawWindow(::Core *core);

} // namespace tomba2::horizontal_cull
