// update_block_draws.h — 2D packets the update block links before the draw section (the letterbox, panels), rebuilt in
// each replay from the values they were built from, blended.
#pragma once

#include "emit_memory.h"
#include "motion_set.h"

#include <array>
#include <cstdint>
#include <vector>

class Core;

namespace tomba {

class UpdateBlockDraws final : public psx::present::MotionSource {
public:
  struct Draw {
    std::uint32_t writer = 0;               // guest entry
    std::uint32_t object = 0;               // the writer's key: its slot or its call-site object
    std::array<std::uint32_t, 2> packets{}; // two packets, or a run's first and its end, as its rebuild reads them
    std::array<std::uint32_t, 4> values{};
    // Rewrites `later`'s packets, read from `memory`, `t` of the way from `earlier`'s values.
    void (*rebuild)(const psx::present::EmitMemory &memory,
                    psx::present::GuestRedraw &redraw,
                    const Draw &earlier,
                    const Draw &later,
                    float t) = nullptr;
  };

  // A writer linked `draw` into the frame's OT; kept only between the frame's start and the section's sample.
  void note(Core &core, const Draw &draw);
  // A writer with no guest owner for its call drew in the update block; it is shown as drawn and counted at run end.
  void unowned(Core &core, std::uint32_t writer, std::uint32_t returnAddress);
  // A frame begins: nothing it notes before the sample is from the last.
  void open();

  void report() const;

  [[nodiscard]] const char *name() const override {
    return "update-block 2D";
  }
  void sample(Core &core) override;
  psx::present::MotionCount write(psx::present::GuestRedraw &redraw, float t) const override;

private:
  std::vector<Draw> noting_;
  std::vector<Draw> earlier_;
  std::vector<Draw> later_;
  bool open_ = false;
  std::uint64_t unowned_ = 0;
  Core *core_ = nullptr; // the sampled frame's, which the rebuilds read
};

} // namespace tomba
