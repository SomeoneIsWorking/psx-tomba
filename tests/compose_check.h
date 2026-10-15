// One logic frame drawn through the shipping writers, and what composeFrame makes of two of them: the checks the
// writer producer tests share.
#pragma once

#include "core.h"
#include "core/entry/session.h"
#include "frame_composer.h"
#include "render/guest_ordering_table.h"
#include "state_producer.h"
#include "state_render_check.h"

#include <cstdint>
#include <utility>
#include <variant>
#include <vector>

namespace tomba::test {

struct DrawnFrame {
  psx::present::FrameRecord shown;
  psx::present::FrameState state;
};

class ComposeHarness {
public:
  ComposeHarness(Core &core, std::uint32_t ot, std::uint32_t pool) : core_(core), ot_(ot), pool_(pool) {}

  psx::present::StateProducerTable &renders() {
    return renders_;
  }

  // An empty OT and pool, a new logic frame, then `draw`; the frame the walk sees and the states it names.
  template <typename Draw> DrawnFrame frame(Draw draw) {
    core_.mem_w32(render::OrderingTable::kBasePointer, ot_);
    clearOrderingTable(core_, ot_);
    render::PacketPool(core_).setCursor(pool_);
    sessionOf(&core_).callSites.beginFrame();
    core_.frameStates.clear();
    draw();
    psx::present::FrameRecord shown = submittedFrame(core_, ot_);
    // The device sampled every CLUT the guest drew through.
    for (auto &entry : shown.entries()) {
      if (auto *primitive = std::get_if<psx::present::DrawPrimitive>(&entry);
          primitive != nullptr && primitive->textured) {
        primitive->clutOffset = 0u;
      }
    }
    psx::present::FrameState state = core_.frameStates.collect(shown);
    return {std::move(shown), std::move(state)};
  }

  // t = 1 composes `to` as drawn.
  bool composesAsDrawn(const DrawnFrame &from, const DrawnFrame &to) const {
    const auto composed = psx::present::composeFrame(to.shown, &from.state, to.state, 1.0f, renders_);
    return !primitives(to.shown).empty() && composed && sameFrame(primitives(*composed), primitives(to.shown));
  }
  // t = 0.5 composes `midway`, a frame unlike `to`.
  bool composesBetween(const DrawnFrame &from, const DrawnFrame &to, const psx::present::FrameRecord &midway) const {
    const auto composed = psx::present::composeFrame(to.shown, &from.state, to.state, 0.5f, renders_);
    return composed && sameFrame(primitives(*composed), primitives(midway)) &&
           !sameFrame(primitives(midway), primitives(to.shown));
  }

  static std::vector<psx::present::DrawPrimitive> primitives(const psx::present::FrameRecord &record) {
    std::vector<psx::present::DrawPrimitive> found;
    for (const auto &entry : record.entries()) {
      if (const auto *primitive = std::get_if<psx::present::DrawPrimitive>(&entry)) {
        found.push_back(*primitive);
      }
    }
    return found;
  }

private:
  Core &core_;
  std::uint32_t ot_;
  std::uint32_t pool_;
  psx::present::StateProducerTable renders_;
};

} // namespace tomba::test
