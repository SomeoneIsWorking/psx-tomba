// LetterboxBars (FUN_80026864): the bar height machine and the two rects, at 4:3 and with a margin.
#include "game.h"
#include "guest_ordering_table.h"
#include "letterbox_bars.h"
#include "stub_runtime.h"

#include <cstdint>
#include <lucent/log.h>
#include <memory>

namespace {

using tomba2::render::LetterboxBars;
using tomba2::render::OrderingTable;
using tomba2::render::PacketPool;

constexpr std::uint32_t kSlot = 0x80100400u;
constexpr std::uint32_t kOt = 0x801B0000u;
constexpr std::uint32_t kPool = 0x801A0000u;
constexpr std::uint32_t kNearBucket = 1u;
constexpr std::uint32_t kChainEnd = 0x00FFFFFFu;
constexpr int kMargin16x9 = 54;

int failed = 0;
int checked = 0;

void check(bool condition, const char *name) {
  ++checked;
  if (!condition) {
    ++failed;
    lucent::error("letterbox-bars-test", "failed: {}", name);
  }
}

struct Rect {
  std::uint32_t command;
  std::int16_t x, y, w, h;
};

Rect rectAt(Core &core, std::uint32_t packet) {
  return {core.mem_r32(packet + 4u),
          static_cast<std::int16_t>(core.mem_r16(packet + 8u)),
          static_cast<std::int16_t>(core.mem_r16(packet + 10u)),
          static_cast<std::int16_t>(core.mem_r16(packet + 12u)),
          static_cast<std::int16_t>(core.mem_r16(packet + 14u))};
}

void reset(Core &core) {
  core.mem_w32(OrderingTable::kBasePointer, kOt);
  core.mem_w32(kOt + kNearBucket * 4u, kChainEnd);
  PacketPool(core).setCursor(kPool);
  core.mem_w8(LetterboxBars::kEffectsArmed, 2);
  core.mem_w8(kSlot + LetterboxBars::kState, 0);
  core.mem_w16(kSlot + LetterboxBars::kHeight, 0);
}

std::int16_t height(Core &core) {
  return static_cast<std::int16_t>(core.mem_r16(kSlot + LetterboxBars::kHeight));
}

void testHeightMachine(Core &core) {
  reset(core);
  core.mem_w8(LetterboxBars::kCue, 0);
  LetterboxBars::tick(core, kSlot, 0);
  check(core.mem_r8(kSlot + LetterboxBars::kState) == 0, "idle waits for the open cue");
  core.mem_w8(LetterboxBars::kCue, 1);
  LetterboxBars::tick(core, kSlot, 0);
  check(core.mem_r8(kSlot + LetterboxBars::kState) == 1 && height(core) == 0, "the open cue starts the grow");
  check(PacketPool(core).cursor() == kPool, "arming draws nothing");
  for (int i = 0; i < LetterboxBars::kFullHeight; ++i) {
    LetterboxBars::tick(core, kSlot, 0);
  }
  check(core.mem_r8(kSlot + LetterboxBars::kState) == 2 && height(core) == LetterboxBars::kFullHeight,
        "the bars hold at full height");
  core.mem_w8(LetterboxBars::kCue, 2);
  LetterboxBars::tick(core, kSlot, 0);
  check(core.mem_r8(kSlot + LetterboxBars::kState) == 3 && height(core) == LetterboxBars::kFullHeight,
        "the end cue starts the shrink");
  for (int i = 0; i < LetterboxBars::kFullHeight; ++i) {
    LetterboxBars::tick(core, kSlot, 0);
  }
  check(core.mem_r8(kSlot + LetterboxBars::kState) == 0 && height(core) == 0, "the shrink ends idle");
  core.mem_w8(LetterboxBars::kEffectsArmed, 1);
  core.mem_w8(LetterboxBars::kCue, 1);
  LetterboxBars::tick(core, kSlot, 0);
  check(core.mem_r8(kSlot + LetterboxBars::kState) == 0, "nothing runs unless the effects are armed");
}

void testRects(Core &core, int margin) {
  reset(core);
  core.mem_w8(kSlot + LetterboxBars::kState, 2);
  core.mem_w16(kSlot + LetterboxBars::kHeight, LetterboxBars::kFullHeight);
  core.mem_w8(LetterboxBars::kCue, 1);
  LetterboxBars::tick(core, kSlot, margin);
  check(PacketPool(core).cursor() == kPool + 32u, "two rect packets");
  const Rect top = rectAt(core, kPool);
  const Rect bottom = rectAt(core, kPool + 16u);
  const std::int16_t x = static_cast<std::int16_t>(-margin);
  const std::int16_t w = static_cast<std::int16_t>(320 + 2 * margin);
  check(top.command == 0x60000000u && bottom.command == 0x60000000u, "black flat rects");
  check(top.x == x && top.w == w && top.y == 0 && top.h == LetterboxBars::kFullHeight, "the top bar spans the canvas");
  check(bottom.x == x && bottom.w == w && bottom.y == LetterboxBars::kGuestHeight - LetterboxBars::kFullHeight &&
            bottom.h == LetterboxBars::kFullHeight,
        "the bottom bar spans the canvas");
  check(core.mem_r32(kOt + kNearBucket * 4u) == kPool + 16u, "both link into the near bucket");
}

} // namespace

int main() {
  tomba::test::StubRuntime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  testHeightMachine(core);
  testRects(core, 0);
  testRects(core, kMargin16x9);
  lucent::info("letterbox-bars-test", "checked={} failed={}", checked, failed);
  return failed == 0 ? 0 : 1;
}
