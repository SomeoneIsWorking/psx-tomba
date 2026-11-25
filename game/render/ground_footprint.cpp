// game/render/ground_footprint.cpp — GroundFootprint, see ground_footprint.h.
#include "ground_footprint.h"

#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "native_dispatch.h"
#include "wide_window.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <string_view>
#include <utility>

namespace tomba::render {

namespace {

struct Gather {
  std::string_view image;
  std::uint32_t address;
};

// The gather each area's prepass calls (tomba2-decomp: the RUN table's entries, A09 has none).
constexpr std::array<Gather, 21> kGathers{{
    {"A00", 0x8013F4DCu}, {"A01", 0x8012F188u}, {"A02", 0x80124138u}, {"A03", 0x8010EC34u}, {"A04", 0x801170A8u},
    {"A05", 0x80136788u}, {"A06", 0x8013DA8Cu}, {"A07", 0x8012E610u}, {"A08", 0x8012AEC0u}, {"A0A", 0x80110CF8u},
    {"A0B", 0x80113A28u}, {"A0C", 0x8011478Cu}, {"A0D", 0x80114020u}, {"A0E", 0x80114D30u}, {"A0F", 0x80115FF4u},
    {"A0G", 0x8010CCDCu}, {"A0H", 0x8010BFBCu}, {"A0I", 0x8010C440u}, {"A0J", 0x8010B420u}, {"A0K", 0x80116E68u},
    {"A0L", 0x8010B61Cu},
}};

std::int32_t away(double from, double to) {
  return static_cast<std::int32_t>(to >= from ? std::ceil(to) : std::floor(to));
}

GroundFootprint::Cell readCell(Core *c, std::uint32_t at) {
  return {.x = static_cast<std::int32_t>(c->mem_r32(at)), .z = static_cast<std::int32_t>(c->mem_r32(at + 4u))};
}

void writeCell(Core *c, std::uint32_t at, GroundFootprint::Cell cell) {
  c->mem_w32(at, static_cast<std::uint32_t>(cell.x));
  c->mem_w32(at + 4u, static_cast<std::uint32_t>(cell.z));
}

template <std::size_t I> void gather(Core *c) {
  GroundFootprint::Cell left = readCell(c, c->r[6]);
  GroundFootprint::Cell right = readCell(c, c->r[7]);
  GroundFootprint::open(c, left, right);
  writeCell(c, c->r[6], left);
  writeCell(c, c->r[7], right);
  psx::cpu::callOriginalToReturn(*c, kGathers[I].address, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
}

template <std::size_t... I>
void declareGathers(native::NativeOverrideCatalog &natives, std::index_sequence<I...> /*unused*/) {
  (natives.declareOverlay(kGathers[I].image, kGathers[I].address, "GroundFootprint::gather", &gather<I>), ...);
}

} // namespace

void GroundFootprint::open(Core *c, Cell &left, Cell &right) {
  const int margin = wide_window::marginColumns(c);
  if (margin == 0) {
    return;
  }
  const double half = wide_window::kGuestWidth / 2.0;
  const double ratio = (half + margin) / half;
  const double midX = (left.x + right.x) / 2.0;
  const double midZ = (left.z + right.z) / 2.0;
  const auto widen = [&](Cell &cell) {
    cell = {.x = away(cell.x, midX + ((cell.x - midX) * ratio)), .z = away(cell.z, midZ + ((cell.z - midZ) * ratio))};
  };
  widen(left);
  widen(right);
}

void GroundFootprint::registerOverrides(native::NativeOverrideCatalog &natives) {
  declareGathers(natives, std::make_index_sequence<kGathers.size()>{});
}

} // namespace tomba::render
