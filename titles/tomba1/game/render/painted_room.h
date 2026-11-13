// Tomba! 1's widescreen projection: a painted room is one picture wider than the buffer, so its frames hold picture
// only as far as the picture's own edges.
#pragma once

#include "guest_widescreen_projection.h"

namespace tomba1 {

class PaintedRoomProjection final : public psx::present::GuestWidescreenProjection {
public:
  // While a painted room's picture is drawn, the picture's columns beyond the 320-column buffer; otherwise unbounded.
  psx::present::PictureBeyondBuffer pictureBeyondBuffer(Core &core) const override;
};

} // namespace tomba1
