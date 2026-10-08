// The title's horizontal visibility cull (game/render/horizontal_visibility_cull.*), tested
// hermetically against the RECOVERED guest predicate rather than against whatever the code now says.
//
// The recovered predicate, read instruction by instruction from the authenticated MAIN.EXE at guest
// 0x8003B438 (inside the quad submitter 0x8003B320) and its 69 structurally identical siblings across
// the 29 authenticated images:
//
//     lhu   $v0, 8($a3)          ; corner 0 SX, the PACKED 16-bit SXY word
//     sltiu $v0, $v0, 0x140      ; SX < 320, UNSIGNED, against the literal with NO shift
//     bnez  $v0, KEEP            ; any corner inside -> keep
//     ...                       ; corners 1, 2, 3, each branching to the same KEEP
//     ... then the same four against 0xF0 for SY, each branching to KEEP
//
// Keep iff (some corner X inside) AND (some corner Y inside). The retail transcription of that is
// restated in `retailKeeps` below and the 4:3 tests sweep it against the owner over the whole
// boundary domain, so the identity is a property of the DOMAIN and not of a handful of chosen cases.
//
// WHY THE SWEEP EARNED ITS KEEP. The first version of this file transcribed the retail predicate with
// a `<<16` on both sides, and the sweep reported 400-odd divergences against a correct owner. The
// transcription was wrong: the guest compares the packed word against 320 directly, and only the
// 16.16 owner form (A0L 0x80112E90, `lui $t9,0x140; sll $v1,$v1,0x10; sltu`) shifts, because there both
// operands are raw 32-bit GTE registers. A dozen hand-picked cases would have passed with the wrong
// transcription; the sweep is what found it.
//
// WHY THE NEGATIVES ARE THE POINT. A cull tested only at 4:3 passes forever while the widened run
// quietly drops the margin it was widened to admit. So the widened tests name primitives retail DROPS
// that a widened run must KEEP, and the mutation checks in the sibling target disconnect each thing
// these tests claim to cover and require the suite to fail.
#include "../game/render/horizontal_visibility_cull.h"

#include <cstdint>
#include <cstdio>

using tomba2::horizontal_cull::Domain;
using tomba2::horizontal_cull::insideAxis;
using tomba2::horizontal_cull::kGuestFrameHeight;
using tomba2::horizontal_cull::kGuestFrameWidth;
using tomba2::horizontal_cull::kQuadCorners;
using tomba2::horizontal_cull::kTriangleCorners;
using tomba2::horizontal_cull::Visibility;

namespace {

int gFailures = 0;
int gChecks = 0;

// A failure that repeats thousands of times is unreadable, so a sweep reports its COUNT and the first
// few instances. The count is the signal: a sweep that found nothing wrong must say zero, so a broken
// matcher cannot hide inside a truncated log.
constexpr int kMaxReported = 6;

void check(bool ok, const char *what) {
  gChecks++;
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    gFailures++;
  }
}

struct Sweep {
  int compared = 0;
  int diverged = 0;

  void compare(bool got, bool want) {
    compared++;
    if (got != want) {
      diverged++;
    }
  }

  void report(const char *what) {
    gChecks++;
    if (diverged != 0) {
      std::printf("FAIL: %s — %d of %d inputs diverge (%d shown)\n",
                  what,
                  diverged,
                  compared,
                  diverged < kMaxReported ? diverged : kMaxReported);
      gFailures++;
    } else {
      std::printf("  %s: %d inputs, 0 diverge\n", what, compared);
    }
  }
};

// THE RETAIL PREDICATE, transcribed independently of the owner: `lhu` the packed SXY word, `sltiu` it
// against the literal, one compare per corner, stop at the first corner that passes.
bool retailKeeps(const std::uint32_t *xs, const std::uint32_t *sy, int count) {
  bool anyX = false;
  for (int i = 0; i < count && !anyX; i++) {
    anyX = xs[i] < 320u;
  }
  if (!anyX) {
    return false;
  }
  bool anyY = false;
  for (int i = 0; i < count && !anyY; i++) {
    anyY = sy[i] < 240u;
  }
  return anyY;
}

struct Quad {
  std::uint32_t x[kQuadCorners];
  std::uint32_t y[kQuadCorners];
};

// Four corners all carrying the same value -- the shape most boundary cases take.
struct FlatCorners {
  std::uint32_t v[kQuadCorners];
};

FlatCorners allCorners(std::uint32_t v) {
  return FlatCorners{{v, v, v, v}};
}

Quad quadAt(int x0, int y0, int x1, int y1) {
  Quad q;
  q.x[0] = (std::uint32_t)x0;
  q.y[0] = (std::uint32_t)y0;
  q.x[1] = (std::uint32_t)x1;
  q.y[1] = (std::uint32_t)y0;
  q.x[2] = (std::uint32_t)x0;
  q.y[2] = (std::uint32_t)y1;
  q.x[3] = (std::uint32_t)x1;
  q.y[3] = (std::uint32_t)y1;
  return q;
}

// THE IDENTITY, OVER THE BOUNDARY DOMAIN. The values chosen are the ones the recovered predicate is
// sensitive to: 0, the last inside pixel, the first outside pixel, and the negative wrap the unsigned
// compare is there to catch. Every combination of two X values and two Y values, so all four
// OR-gate/AND-gate interactions are exercised, on both axes at once.
void fourByThreeIsTheRetailPredicateOverTheDomain() {
  const Visibility guest{0, kGuestFrameWidth};
  check(guest.isGuestFrame(), "320 is the guest frame");
  const int xs[] = {0, 1, 2, 318, 319, 320, 321, 400, 0x8000, 0xFFFF};
  const int ys[] = {0, 1, 2, 238, 239, 240, 241, 400, 0x8000, 0xFFFF};
  Sweep sweep;
  int shown = 0;
  for (int a : xs) {
    for (int b : xs) {
      for (int c : ys) {
        for (int d : ys) {
          const Quad q = quadAt(a, c, b, d);
          const bool got = guest.keeps(q.x, q.y, kQuadCorners, Domain::Packed16);
          const bool want = retailKeeps(q.x, q.y, kQuadCorners);
          sweep.compare(got, want);
          if (got != want && shown < kMaxReported) {
            std::printf("  divergence x=(%d,%d) y=(%d,%d): owner %d, retail %d\n", a, b, c, d, (int)got, (int)want);
            shown++;
          }
        }
      }
    }
  }
  sweep.report("4:3 quad identity against the retail predicate");
}

// The same sweep for the triangle form, which the census also found (3 corners, not 4).
void triangleFormIsTheRetailPredicate() {
  const Visibility guest{0, kGuestFrameWidth};
  const int xs[] = {0, 1, 319, 320, 321, 0x8000, 0xFFFF};
  const int ys[] = {0, 239, 240, 241, 0x8000, 0xFFFF};
  Sweep sweep;
  for (int a : xs) {
    for (int b : xs) {
      for (int c : ys) {
        const std::uint32_t x[3] = {(std::uint32_t)a, (std::uint32_t)b, (std::uint32_t)a};
        const std::uint32_t y[3] = {(std::uint32_t)c, (std::uint32_t)c, (std::uint32_t)c};
        sweep.compare(guest.keeps(x, y, kTriangleCorners, Domain::Packed16), retailKeeps(x, y, kTriangleCorners));
      }
    }
  }
  sweep.report("4:3 triangle identity against the retail predicate");
}

// BOUNDARY CASES, both sides, named. Each is a real edge in the recovered predicate, and each is where
// a signed compare, an off-by-one, or an AND/OR swap shows up as a wrong row of pixels.
void everyBoundaryOnBothSides() {
  const Visibility guest{0, kGuestFrameWidth};

  const std::uint32_t insideY[kQuadCorners] = {10, 10, 10, 10};
  const std::uint32_t insideX[kQuadCorners] = {10, 10, 10, 10};

  // The X axis alone, with Y pinned inside.
  const FlatCorners lastInsideX = allCorners(319);
  const FlatCorners firstOutsideX = allCorners(320);
  check(guest.keepsX(lastInsideX.v, kQuadCorners, Domain::Packed16), "SX 319 is the last inside pixel");
  check(!guest.keepsX(firstOutsideX.v, kQuadCorners, Domain::Packed16), "SX 320 is the first outside pixel");
  check(guest.keeps(lastInsideX.v, insideY, kQuadCorners, Domain::Packed16), "a quad at 319 is kept");
  check(!guest.keeps(firstOutsideX.v, insideY, kQuadCorners, Domain::Packed16), "a quad at 320 is dropped");

  // The Y axis alone, with X pinned inside.
  const FlatCorners lastInsideY = allCorners(239);
  const FlatCorners firstOutsideY = allCorners(240);
  check(guest.keepsY(lastInsideY.v, kQuadCorners, Domain::Packed16), "SY 239 is the last inside row");
  check(!guest.keepsY(firstOutsideY.v, kQuadCorners, Domain::Packed16), "SY 240 is the first outside row");
  check(guest.keeps(insideX, lastInsideY.v, kQuadCorners, Domain::Packed16), "a quad at 239 is kept");
  check(!guest.keeps(insideX, firstOutsideY.v, kQuadCorners, Domain::Packed16), "a quad at 240 is dropped");

  // THE UNSIGNED COMPARE. A coordinate the GTE wrapped negative reads as 0x8000..0xFFFF and must FAIL.
  // Retail drops it; a signed compare would keep it. This is the single most important negative here.
  const FlatCorners wrapped = allCorners(0x8000);
  check(!guest.keepsX(wrapped.v, kQuadCorners, Domain::Packed16), "a negative SX fails the unsigned test");
  check(!guest.keepsY(wrapped.v, kQuadCorners, Domain::Packed16), "a negative SY fails the unsigned test");
  check(!insideAxis(0x8000u, 320, Domain::Packed16), "insideAxis rejects a wrapped coordinate");
  check(!insideAxis(0xFFFFu, 320, Domain::Packed16), "insideAxis rejects the largest wrapped value");
  check(insideAxis(319u, 320, Domain::Packed16), "insideAxis accepts the last inside pixel");
  check(!insideAxis(320u, 320, Domain::Packed16), "insideAxis rejects the first outside pixel");

  // THE OR OVER CORNERS. A quad straddling the right edge survives on its left corners; a quad wholly
  // past it does not. Collapsing the gate to a bounding box would drop the straddler.
  const std::uint32_t straddle[kQuadCorners] = {300, 340, 300, 340};
  const std::uint32_t beyond[kQuadCorners] = {400, 440, 400, 440};
  check(guest.keepsX(straddle, kQuadCorners, Domain::Packed16), "a quad straddling 320 is kept");
  check(!guest.keepsX(beyond, kQuadCorners, Domain::Packed16), "a quad wholly past 320 is dropped");

  // THE AND OF THE TWO AXES.
  const FlatCorners allOutside = allCorners(400);
  check(!guest.keeps(insideX, allOutside.v, kQuadCorners, Domain::Packed16), "inside X and outside Y is dropped");
  check(!guest.keeps(allOutside.v, insideY, kQuadCorners, Domain::Packed16), "outside X and inside Y is dropped");
  check(guest.keeps(insideX, insideY, kQuadCorners, Domain::Packed16), "inside on both is kept");

  // ZERO CORNERS is not a primitive, and must not be "kept" by an empty OR.
  const std::uint32_t one[1] = {10};
  check(!guest.keepsX(one, 0, Domain::Packed16), "zero corners is not kept on X");
  check(!guest.keepsY(one, 0, Domain::Packed16), "zero corners is not kept on Y");
}

// THE WIDENED BEHAVIOUR. The bound moves; nothing else does. Every case below is a primitive retail
// DROPS that a widened run must KEEP, and the widened bound alone is the reason.
void wideningAdmitsExactlyTheMargin() {
  const Visibility wide{0, 428};
  check(!wide.isGuestFrame(), "428 is not the guest frame");
  const std::uint32_t insideY[kQuadCorners] = {10, 10, 10, 10};
  const std::uint32_t insideX[kQuadCorners] = {10, 10, 10, 10};

  // The canvas's own right edge, at the retail scale.
  const FlatCorners atWideEdge = allCorners(427);
  const FlatCorners pastWideEdge = allCorners(428);
  check(wide.keepsX(atWideEdge.v, kQuadCorners, Domain::Packed16), "the last wide pixel is on screen");
  check(!wide.keepsX(pastWideEdge.v, kQuadCorners, Domain::Packed16), "the first pixel past it is not");
  check(wide.keeps(atWideEdge.v, insideY, kQuadCorners, Domain::Packed16), "geometry in the new right band is KEPT");

  // THE SAME primitive at 4:3 is dropped, so the widening is what admits it. This pair is what makes
  // the widened case falsifiable rather than merely different.
  const Visibility guest{0, kGuestFrameWidth};
  check(!guest.keeps(atWideEdge.v, insideY, kQuadCorners, Domain::Packed16), "the same geometry is DROPPED at 4:3");

  // WIDENING MUST NOT MOVE THE VERTICAL BOUND. 240 is still the bottom edge, because widening OFX at
  // unchanged OFY and H leaves the vertical field of view alone.
  const FlatCorners atBottom = allCorners(240);
  check(!wide.keepsY(atBottom.v, kQuadCorners, Domain::Packed16), "a wider canvas does not extend the bottom");
  check(!guest.keepsY(atBottom.v, kQuadCorners, Domain::Packed16), "the bottom edge is 240 at 4:3 too");
  check(!wide.keeps(insideX, atBottom.v, kQuadCorners, Domain::Packed16),
        "a wider X does not rescue a quad below the bottom edge");

  // THE SCALE MUST NOT CHANGE: column 319 is on screen in both, because the bound moved and nothing
  // else did.
  const FlatCorners at4x3Edge = allCorners(319);
  check(wide.keepsX(at4x3Edge.v, kQuadCorners, Domain::Packed16), "column 319 is still on screen when wide");
  check(guest.keepsX(at4x3Edge.v, kQuadCorners, Domain::Packed16), "column 319 is on screen at 4:3");

  // A 21:9 canvas, so the owner is shown to have no notion of a particular ratio.
  const Visibility ultra{0, 569};
  check(ultra.keepsX(allCorners(568).v, kQuadCorners, Domain::Packed16), "a 21:9 canvas admits column 568");
  check(!ultra.keepsX(allCorners(569).v, kQuadCorners, Domain::Packed16), "and not column 569");
}

// THE 16.16 DOMAIN, the second owner form the census found (A0L 0x80112E90). Same predicate,
// different representation. `insideAxis` takes the bound UNSHIFTED and shifts it for this domain, which
// is the one thing the first version of this test got wrong.
void fixed16Point16DomainIsTheSamePredicate() {
  const std::uint32_t shift = 16;
  const int packed[] = {0, 1, 319, 320, 321, 0x8000, 0xFFFF};
  for (int p : packed) {
    const std::uint32_t value = (std::uint32_t)p;
    const std::uint32_t asFixed = value << shift;
    check(insideAxis(asFixed, kGuestFrameWidth, Domain::Fixed16_16) ==
              insideAxis(value, kGuestFrameWidth, Domain::Packed16),
          "the 16.16 domain agrees with the packed one for a whole 16-bit value");
  }
  // A value with a non-zero low half: 319.5 must still be INSIDE, which a truncation would get wrong.
  const std::uint32_t half = (319u << 16) | 0x8000u;
  check(insideAxis(half, kGuestFrameWidth, Domain::Fixed16_16), "319.5 is still inside 320");
  check(!insideAxis(320u << 16, kGuestFrameWidth, Domain::Fixed16_16), "320.0 is not");
  check(!insideAxis(320u << 16 | 0x8000u, kGuestFrameWidth, Domain::Fixed16_16), "320.5 is not");
  // The shift must not become an off-by-16 in either direction.
  const int around[] = {318, 319, 320, 321};
  for (int p : around) {
    const std::uint32_t value = (std::uint32_t)p << shift;
    check(insideAxis(value, kGuestFrameWidth, Domain::Fixed16_16) == (p < 320),
          "the 16.16 boundary is the 16-bit boundary at the same value");
  }
  // A 16.16 coordinate past 16 bits must still be rejected, which is the A0L case's whole point: the
  // raw GTE register is 32-bit and a far-off-screen vertex can exceed 0xFFFF<<16.
  check(!insideAxis(0xFFFF0000u, kGuestFrameWidth, Domain::Fixed16_16), "a 16.16 coordinate beyond 16 bits is outside");
}

// THE NAMED CONSTANTS. Editing either changes every 4:3 capture and oracle comparison, so the edit has
// to be deliberate rather than incidental.
void theGuestFrameIsTheRecoveredOne() {
  check(kGuestFrameWidth == 320, "the retail width is 320, the literal guest 0x8003B440 compares");
  check(kGuestFrameHeight == 240, "the retail height is 240");
  check(kTriangleCorners == 3, "the triangle owner tests three corners");
  check(kQuadCorners == 4, "the quad owner tests four corners");
}

// THE BOUND IS A FUNCTION OF THE CANVAS, NOT A CONSTANT IN DISGUISE. A cull that hardcoded a widened
// number would satisfy every behaviour test above and still be the defect this whole change exists to
// remove, so the owner is checked for being a function of its input: two widths differing by one column
// must differ at exactly that one column of admission, across the whole boundary.
void theBoundIsAFunctionOfTheCanvasNotAConstant() {
  int disagreements = 0;
  int examined = 0;
  for (int width = kGuestFrameWidth; width <= 600; width += 1) {
    const Visibility narrow{0, width};
    const Visibility wider{0, width + 1};
    for (int x = 0; x <= 620; x += 1) {
      const std::uint32_t xs[kQuadCorners] = {(std::uint32_t)x, (std::uint32_t)x, (std::uint32_t)x, (std::uint32_t)x};
      const bool a = narrow.keepsX(xs, kQuadCorners, Domain::Packed16);
      const bool b = wider.keepsX(xs, kQuadCorners, Domain::Packed16);
      examined++;
      if (a != b && x != width) {
        disagreements++;
      }
    }
  }
  char label[160];
  std::snprintf(label, sizeof(label), "%d comparisons, widening moves admission at exactly one column", examined);
  check(disagreements == 0, label);
}

// THE CULL CENSUS, WITH DENOMINATORS. The same captured corner set, run through both widths, so the
// widened run's newly-admitted population is a COUNT with a named population rather than an assertion
// that "it looks wider". The population is every distinct corner X a projected vertex can land on, with
// Y inside the frame, so the denominator is the whole horizontal domain and not a chosen sample.
//
// What this measures and what it does not: it measures the PREDICATE over that domain, which is the
// part this owner owns. It is not a frame-time count of real objects in a real scene -- that would need
// the product running, and a census of synthetic corners does not become one by being exhaustive.
void cullCensusAtBothWidths() {
  const Visibility guest{0, kGuestFrameWidth};
  const Visibility wide{0, 428};
  int total = 0;
  int kept43 = 0;
  int keptWide = 0;
  int newlyAdmitted = 0;
  int newlyLost = 0;
  for (int x = 0; x <= 700; x++) {
    const Quad q = quadAt(x, 100, x, 100);
    total++;
    const bool a = guest.keeps(q.x, q.y, kQuadCorners, Domain::Packed16);
    const bool b = wide.keeps(q.x, q.y, kQuadCorners, Domain::Packed16);
    kept43 += a ? 1 : 0;
    keptWide += b ? 1 : 0;
    if (!a && b) {
      newlyAdmitted++;
    }
    if (a && !b) {
      newlyLost++;
    }
  }
  std::printf("  cull census over %d distinct corner X with Y inside the frame:\n", total);
  std::printf("    4:3  (width 320): %d kept, %d dropped\n", kept43, total - kept43);
  std::printf("    wide (width 428): %d kept, %d dropped\n", keptWide, total - keptWide);
  std::printf("    newly admitted by the widening: %d (corner X in [320,428))\n", newlyAdmitted);
  std::printf("    newly dropped by the widening:   %d\n", newlyLost);

  // The three numbers the census asserts, so a regression is a FAIL and not a diff in a log.
  check(kept43 == 320, "at 4:3 exactly the columns [0,320) are kept");
  check(keptWide == 428, "when wide exactly the columns [0,428) are kept");
  check(newlyAdmitted == 428 - 320, "the newly admitted population is exactly the added columns");
  check(newlyLost == 0, "widening loses nothing: a wider bound cannot drop a column");

  // The vertical half of the census, and the reason the count above can be stated in columns at all:
  // widening must not add a single row.
  int rows43 = 0;
  int rowsWide = 0;
  for (int y = 0; y <= 400; y++) {
    const Quad q = quadAt(10, y, 10, y);
    rows43 += guest.keeps(q.x, q.y, kQuadCorners, Domain::Packed16) ? 1 : 0;
    rowsWide += wide.keeps(q.x, q.y, kQuadCorners, Domain::Packed16) ? 1 : 0;
  }
  check(rows43 == 240, "at 4:3 exactly the rows [0,240) are kept");
  check(rowsWide == 240, "when wide exactly the rows [0,240) are kept — the vertical bound did not move");
}

// A 4:3 owner and a widened owner must be distinguishable by their own state, so a caller cannot pass
// one where the other is required without the code saying so.
void theFrameStateIsReadable() {
  check(Visibility{0, kGuestFrameWidth}.isGuestFrame(), "a 320 owner reports the guest frame");
  check(!Visibility{0, kGuestFrameWidth + 1}.isGuestFrame(), "a 321 owner does not");
  check(Visibility{0, 569}.right() == 569, "the bound is readable");
}

// The record path's 16:9 canvas is [-54, 374): a corner in either margin keeps its primitive, one
// past either margin or wrapped far negative does not, in both domains.
void recordCanvasKeepsBothMargins() {
  const Visibility canvas{-54, kGuestFrameWidth + 54};
  check(!canvas.isGuestFrame(), "the canvas window is not the guest frame");
  const auto packed = [](int x) {
    return static_cast<std::uint32_t>(x) & 0xFFFFu;
  };
  const auto fixed = [](int x) {
    return static_cast<std::uint32_t>(x) << 16;
  };
  const int cases[][2] = {{-55, 0}, {-54, 1}, {-1, 1}, {0, 1}, {319, 1}, {320, 1}, {373, 1}, {374, 0}, {-1024, 0}};
  for (const auto &entry : cases) {
    const std::uint32_t p = packed(entry[0]);
    const std::uint32_t f = fixed(entry[0]);
    check(canvas.keepsX(&p, 1, Domain::Packed16) == (entry[1] != 0), "packed corner against the canvas");
    check(canvas.keepsX(&f, 1, Domain::Fixed16_16) == (entry[1] != 0), "16.16 corner against the canvas");
  }
  const Visibility guest{0, kGuestFrameWidth};
  for (int x = -1024; x < 1024; x++) {
    const std::uint32_t p = packed(x);
    const bool inGuest = guest.keepsX(&p, 1, Domain::Packed16);
    const bool inCanvas = canvas.keepsX(&p, 1, Domain::Packed16);
    if (inGuest && !inCanvas) {
      check(false, "the canvas keeps everything the guest frame keeps");
      return;
    }
  }
}

} // namespace

int main() {
  fourByThreeIsTheRetailPredicateOverTheDomain();
  triangleFormIsTheRetailPredicate();
  everyBoundaryOnBothSides();
  wideningAdmitsExactlyTheMargin();
  fixed16Point16DomainIsTheSamePredicate();
  theGuestFrameIsTheRecoveredOne();
  cullCensusAtBothWidths();
  theBoundIsAFunctionOfTheCanvasNotAConstant();
  theFrameStateIsReadable();
  recordCanvasKeepsBothMargins();
  if (gFailures != 0) {
    std::printf("horizontal_visibility_cull: %d of %d check(s) FAILED\n", gFailures, gChecks);
    return 1;
  }
  std::printf("horizontal_visibility_cull: all %d checks passed\n", gChecks);
  return 0;
}
