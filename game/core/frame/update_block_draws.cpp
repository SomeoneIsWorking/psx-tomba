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
                  "writer 0x{:08X} object 0x{:X} values 0x{:X} 0x{:X}",
                  draw.writer,
                  draw.object,
                  draw.values[0],
                  draw.values[1]);
  }
  open_ = false;
}

psx::present::MotionCount UpdateBlockDraws::write(psx::present::GuestRedraw & /*redraw*/, float t) const {
  psx::present::MotionCount count{.drawn = later_.size()};
  for (const Draw &later : later_) {
    const auto earlier = std::ranges::find_if(earlier_, [&later](const Draw &draw) {
      return draw.writer == later.writer && draw.object == later.object;
    });
    if (earlier == earlier_.end()) {
      ++count.unpaired;
      continue;
    }
    later.rebuild(*core_, *earlier, later, t);
  }
  return count;
}

} // namespace tomba
