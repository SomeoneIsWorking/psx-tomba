// guestReduce: the guest's scroll reduction is the identity in range and not a canonical wrap outside it.
#include "parallax_scroll.h"
#include "testutil.h"

using tomba::parallax::guestReduce;

// ---- guestReduce: identity in range, NOT a canonical wrap outside it ---------------------------

static void test_guest_reduce_is_the_identity_inside_the_range(void) {
  for (int32_t v = 0; v < 256; v++) {
    CHECK_EQ(guestReduce(v, 256), v);
  }
}

static void test_guest_reduce_is_not_a_canonical_wrap(void) {
  // A true wrap would give 251, 88 and 255.
  CHECK_EQ(guestReduce(-5, 256), -5);
  CHECK_EQ(guestReduce(600, 256), 344);
  CHECK_EQ(guestReduce(-513, 256), -1);
}

static void test_guest_reduce_lands_within_one_modulus_of_the_range(void) {
  // What it DOES guarantee: the result is modulus-equivalent to the input and no further than one
  // modulus outside [0, mod). That is the property the signed-16-bit store depends on.
  const int32_t mod = 126; // the seaside's Y modulus shape: (H*0x8E8)/0x90, not H*16
  int32_t outside = 0;
  for (int32_t v = -600; v <= 600; v++) {
    const int32_t r = guestReduce(v, mod);
    CHECK_EQ((r - v) % mod, 0);
    CHECK(r >= -mod && r < 2 * mod);
    if (r < 0 || r >= mod) {
      outside++;
    }
  }
  // and it really does leave the range often — otherwise the check above proves nothing
  CHECK(outside > 400);
}

int main(void) {
  RUN(guest_reduce_is_the_identity_inside_the_range);
  RUN(guest_reduce_is_not_a_canonical_wrap);
  RUN(guest_reduce_lands_within_one_modulus_of_the_range);
  return pt_summary();
}
