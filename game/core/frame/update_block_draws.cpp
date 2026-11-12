#include "frame/update_block_draws.h"

#include "core.h"
#include "entry/session.h"

#include <lucent/log.h>

#include <algorithm>

namespace tomba {

void UpdateBlockDraws::note(Core &core, const Draw &draw) {
  if (open_ && !sessionOf(&core).drawReplay.frame().replaying()) {
    noting_.push_back(draw);
  }
}

void UpdateBlockDraws::unowned(Core &core, std::uint32_t writer, std::uint32_t returnAddress) {
  if (!open_ || sessionOf(&core).drawReplay.frame().replaying()) {
    return;
  }
  ++unowned_;
  lucent::debug("update-draws", "writer 0x{:08X} from 0x{:08X} has no owner; drawn as it is", writer, returnAddress);
}

void UpdateBlockDraws::report() const {
  lucent::info("update-draws", "run-end: {} update-block 2D call(s) with no owner, shown as drawn", unowned_);
}

void UpdateBlockDraws::open() {
  noting_.clear();
  open_ = true;
}

void UpdateBlockDraws::sample(Core &core) {
  core_ = &core;
  earlier_.swap(later_);
  later_.swap(noting_);
  noting_.clear();
  for (const Draw &draw : later_) {
    lucent::debug("update-draws",
                  "writer 0x{:08X} object 0x{:X} values 0x{:X} 0x{:X} 0x{:X} 0x{:X}",
                  draw.writer,
                  draw.object,
                  draw.values[0],
                  draw.values[1],
                  draw.values[2],
                  draw.values[3]);
  }
  open_ = false;
}

psx::present::MotionCount UpdateBlockDraws::write(psx::present::GuestRedraw &redraw, float t) const {
  psx::present::MotionCount count{.drawn = later_.size()};
  for (const Draw &later : later_) {
    const auto earlier = std::ranges::find_if(earlier_, [&later](const Draw &draw) {
      return draw.writer == later.writer && draw.object == later.object;
    });
    if (earlier == earlier_.end()) {
      ++count.unpaired;
      continue;
    }
    later.rebuild(*core_, redraw, *earlier, later, t);
  }
  return count;
}

} // namespace tomba
