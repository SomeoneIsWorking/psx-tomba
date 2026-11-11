// game/render/view_clip.cpp — ViewClip. See view_clip.h.
#include "view_clip.h"

#include "core.h"
#include "gte_registers.h"
#include "horizontal_visibility_cull.h"

namespace tomba::render {
namespace {

// The part of `polygon` where `inside(point) >= 0`, `inside` linear between its points.
template <class Inside> std::vector<ClipPoint> cut(const std::vector<ClipPoint> &polygon, Inside inside) {
  std::vector<ClipPoint> out;
  for (std::size_t i = 0; i < polygon.size(); ++i) {
    const ClipPoint &a = polygon[i];
    const ClipPoint &b = polygon[(i + 1) % polygon.size()];
    const double da = inside(a);
    const double db = inside(b);
    if (da >= 0.0) {
      out.push_back(a);
    }
    if ((da >= 0.0) != (db >= 0.0)) {
      out.push_back(a.toward(b, da / (da - db)));
    }
  }
  return out;
}

double signedControl(psx::cpu::Gte &gte, std::uint32_t reg) {
  return static_cast<double>(static_cast<std::int32_t>(gte.readControl(reg)));
}

} // namespace

ClipPoint ClipPoint::toward(const ClipPoint &to, double t) const {
  const auto lerp = [t](double from, double end) {
    return from + (end - from) * t;
  };
  return {lerp(x, to.x),
          lerp(y, to.y),
          lerp(z, to.z),
          lerp(u, to.u),
          lerp(v, to.v),
          lerp(r, to.r),
          lerp(g, to.g),
          lerp(b, to.b),
          lerp(viewX, to.viewX),
          lerp(viewY, to.viewY),
          lerp(viewZ, to.viewZ)};
}

std::vector<ClipPoint> ViewClip::place(psx::cpu::Gte &gte, std::span<const ModelPoint> corners) {
  std::vector<ClipPoint> polygon;
  polygon.reserve(corners.size());
  for (const ModelPoint &c : corners) {
    gte.writeData(psx::gte::kVxy0,
                  (static_cast<std::uint32_t>(static_cast<std::uint16_t>(c.y)) << 16) |
                      static_cast<std::uint16_t>(c.x));
    gte.writeData(psx::gte::kVz0, static_cast<std::uint16_t>(c.z));
    gte.execute(psx::gte::kRtps);
    const auto view = [&gte](std::uint32_t reg) {
      return static_cast<double>(static_cast<std::int32_t>(gte.readData(reg)));
    };
    polygon.push_back({double(c.x),
                       double(c.y),
                       double(c.z),
                       double(c.u),
                       double(c.v),
                       double(c.r),
                       double(c.g),
                       double(c.b),
                       view(psx::gte::kMac1),
                       view(psx::gte::kMac2),
                       view(psx::gte::kMac3)});
  }
  return polygon;
}

ViewClip::Screen ViewClip::screen(psx::cpu::Gte &gte, const ClipPoint &point) {
  const double h = static_cast<double>(gte.readControl(psx::gte::kH) & 0xFFFFu);
  return {.x = signedControl(gte, psx::gte::kOfx) / 65536.0 + h * point.viewX / point.viewZ,
          .y = signedControl(gte, psx::gte::kOfy) / 65536.0 + h * point.viewY / point.viewZ};
}

std::vector<ClipPoint> ViewClip::visible(psx::cpu::Gte &gte, std::vector<ClipPoint> polygon, Columns columns) {
  const double h = static_cast<double>(gte.readControl(psx::gte::kH) & 0xFFFFu);
  const double ofx = signedControl(gte, psx::gte::kOfx) / 65536.0;
  const double ofy = signedControl(gte, psx::gte::kOfy) / 65536.0;
  constexpr double kRows = horizontal_cull::kGuestFrameHeight;
  // Each bound is a plane through the eye: screen x = ofx + h * viewX / viewZ, and y likewise.
  polygon = cut(polygon, [h](const ClipPoint &p) {
    return p.viewZ - h / 2.0;
  });
  polygon = cut(polygon, [&](const ClipPoint &p) {
    return h * p.viewX + (ofx - columns.left) * p.viewZ;
  });
  polygon = cut(polygon, [&](const ClipPoint &p) {
    return (columns.right - ofx) * p.viewZ - h * p.viewX;
  });
  polygon = cut(polygon, [&](const ClipPoint &p) {
    return h * p.viewY + ofy * p.viewZ;
  });
  return cut(polygon, [&](const ClipPoint &p) {
    return (kRows - ofy) * p.viewZ - h * p.viewY;
  });
}

} // namespace tomba::render
