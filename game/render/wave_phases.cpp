// game/render/wave_phases.cpp — WavePhases. See wave_phases.h.
#include "render/wave_phases.h"

namespace tomba::render {

void WavePhases::note(std::uint32_t list, std::uint32_t phase) {
  const auto [read, added] = noting_.try_emplace(list, Read{.phase = phase});
  if (!added && read->second.phase != phase) {
    read->second.single = false;
  }
}

std::optional<std::uint32_t> WavePhases::earlier(std::uint32_t list) const {
  const auto read = earlier_.find(list);
  if (read == earlier_.end() || !read->second.single) {
    return std::nullopt;
  }
  return read->second.phase;
}

void WavePhases::sample(Core & /*core*/) {
  unpaired_ = 0;
  for (const auto &[list, read] : noting_) {
    const auto before = earlier_.find(list);
    if (!read.single || before == earlier_.end() || !before->second.single) {
      ++unpaired_;
    }
  }
  earlier_.swap(noting_);
  noting_.clear();
}

psx::present::MotionCount WavePhases::write(psx::present::GuestRedraw & /*redraw*/, float /*t*/) const {
  return {.drawn = earlier_.size(), .unpaired = unpaired_};
}

} // namespace tomba::render
