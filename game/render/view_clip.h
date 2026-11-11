// game/render/view_clip.h — the part of a model polygon the GTE can project exactly, cut in view space.
#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace psx::cpu {
class Gte;
} // namespace psx::cpu

namespace tomba::render {

// A model point with what a packet carries at it and its place in view space.
struct ClipPoint {
  double x, y, z;
  double u, v, r, g, b;
  double viewX, viewY, viewZ;

  // The point `t` of the way to `to`; every field is linear in model space.
  [[nodiscard]] ClipPoint toward(const ClipPoint &to, double t) const;
};

// A model corner as the ground emitters carry it.
struct ModelPoint {
  std::int16_t x, y, z;
  std::uint8_t u, v, r, g, b;
};

class ViewClip {
public:
  // Screen columns [left, right) and the guest's rows; the GTE holds the view.
  struct Columns {
    double left;
    double right;
  };

  // `corners` placed in view space through the GTE's rotation and translation (RTPS per corner).
  static std::vector<ClipPoint> place(psx::cpu::Gte &gte, std::span<const ModelPoint> corners);
  // Where the GTE's projection puts `point` on screen, unrounded.
  struct Screen {
    double x;
    double y;
  };
  static Screen screen(psx::cpu::Gte &gte, const ClipPoint &point);
  // What of `polygon` lies past the GTE's nearest exact depth H / 2 and inside `columns` and the guest's rows.
  static std::vector<ClipPoint> visible(psx::cpu::Gte &gte, std::vector<ClipPoint> polygon, Columns columns);
};

} // namespace tomba::render
