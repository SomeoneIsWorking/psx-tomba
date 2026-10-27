#include "debug/debug_menu.h"

#include "core.h"
#include "debug/dev_gate.h"
#include "debug/diagnostics.h"
#include "engine/engine.h"
#include "entry/session.h"
#include "frame/frame_cut.h"
#include "frame/frame_driver.h"
#include "game.h"
#include "sched/task_slot_layout.h"

#include <lucent/log.h>

#include <array>
#include <cctype>
#include <string_view>

namespace tomba {
namespace {

using psx::game::DebugCall;
using psx::game::DebugNames;
using psx::game::DebugParam;

constexpr std::array<std::string_view, 2> kFlagOps{"get", "set"};
constexpr std::int64_t kCameraLimit = 32767;

DevControl &devControl(Core &core) {
  return core.game->frameDriverAs<FrameDriver>().devControl();
}

std::string fieldRefusal(Core &core) {
  return DevWarp::phase(core) == DevWarp::Phase::Unavailable ? "a warp is legal only while the field is running"
                                                             : std::string();
}

std::string gameStageRefusal(Core &core) {
  return DevGate::inGameStage(core) ? std::string() : "enter the GAME field before changing inventory";
}

} // namespace

DebugMenu::DebugMenu() {
  // A screen is the scheduler's stage and, while the field runs, its area. A field screen is reached by warping there,
  // a stage before GAME by starting the title; the GAME stage outside the field (the opening) has no route.
  screen(
      [](Core &core) {
        const bool field = DevWarp::phase(core) == DevWarp::Phase::Running;
        return (std::uint64_t{core.mem_r32(sched::kTaskTableBase + sched::kTaskEntry)} << 32) |
               (std::uint64_t{field} << 16) | (field ? std::uint64_t{core.mem_r8(FrameCut::kAreaByte)} : 0u);
      },
      [](Core &core, std::uint64_t key) {
        if ((key & 0x10000u) != 0) {
          return lucent::format("warp {}", key & 0xFFu);
        }
        const bool gameStage = static_cast<std::uint32_t>(key >> 32) == sched::kStageGameEntry;
        return !gameStage && core.game->title != nullptr ? lucent::format("pick {}", core.game->title->slug)
                                                         : std::string();
      });
  section("Area Warp");
  readout("Position", [](Core &core) {
    return lucent::format("({}, {}, {})  stage {}",
                          core.mem_r16s(0x1F8000D2u),
                          core.mem_r16s(0x1F8000D6u),
                          core.mem_r16s(0x1F8000DAu),
                          sched::stageName(core.mem_r32(sched::kTaskTableBase + sched::kTaskEntry)));
  });
  verb({
      .name = "warp",
      .summary = "Warp there through the door transition",
      .params = {DebugParam::named("area", DebugNames::of(DevWarp::kAreaNames)),
                 DebugParam::integer("entry", 0, DevWarp::kMaxEntry).orAbsent()},
      .run =
          [](const DebugCall &call) {
            call.reply = devControl(call.core).warp().arm(call.values.u32(0), call.values.u32(1));
          },
      .refusal = fieldRefusal,
  });

  section("Items and Event Flags");
  verb({
      .name = "items-all",
      .summary = "Grant all items",
      .run =
          [](const DebugCall &call) {
            call.reply = devControl(call.core).items().armAll();
          },
      .refusal = gameStageRefusal,
  });
  verb({
      .name = "item",
      .summary = "Give one item",
      .params = {DebugParam::integer("id", 0, DevItems::kMaxItemId),
                 DebugParam::integer("amount", 1, DevItems::kMaxAmount).orAbsent()},
      .run =
          [](const DebugCall &call) {
            call.reply = devControl(call.core).items().armOne(call.values.u32(0), call.values.u32(1, 1));
          },
      .refusal = gameStageRefusal,
  });
  verb({
      .name = "flag",
      .summary = "Read or write an event flag",
      .params = {DebugParam::word("op", DebugNames::of(kFlagOps)),
                 DebugParam::integer("index", 0, DevFlags::kFlagCount - 1),
                 DebugParam::integer("value", 0, 0xFF).orAbsent()},
      .run =
          [](const DebugCall &call) {
            if (call.values.word(0) == kFlagOps[0]) {
              call.reply = DevFlags::get(call.core, call.values.u32(1));
            } else if (!call.values.has(2)) {
              call.reply = "usage: flag set <index> <value>";
            } else {
              call.reply = devControl(call.core).flags().armSet(call.values.u32(1), call.values.u32(2));
            }
          },
  });

  soundTest();

  section("Camera");
  verb({
      .name = "tp",
      .summary = "Pin the camera at x y z; no arguments returns it to the player",
      .params = {DebugParam::integer("x", -kCameraLimit, kCameraLimit).orAbsent(),
                 DebugParam::integer("y", -kCameraLimit, kCameraLimit).orAbsent(),
                 DebugParam::integer("z", -kCameraLimit, kCameraLimit).orAbsent()},
      .run =
          [](const DebugCall &call) {
            if (call.values.has(2)) {
              const int x = static_cast<int>(call.values.value(0));
              const int y = static_cast<int>(call.values.value(1));
              const int z = static_cast<int>(call.values.value(2));
              eng(&call.core).camTeleport({.x = x, .y = y, .z = z});
              call.reply = lucent::format("tp camera -> ({},{},{})", x, y, z);
            } else {
              eng(&call.core).camTeleportOff();
              call.reply = "tp off (camera follows player)";
            }
          },
      .inMenu = false,
  });

  section("Diagnostics");
  const auto logged = [](void (*action)(Core &), std::string_view name) {
    return [action, name](const DebugCall &call) {
      action(call.core);
      call.reply = lucent::format("ok: {}, see the log", name);
    };
  };
  verb({.name = "ents",
        .summary = "List the entity lists to the log",
        .run = logged(&diagnostics::listEntities, "ents"),
        .inMenu = false});
  verb({.name = "stage",
        .summary = "The stage words to the log",
        .run = logged(&diagnostics::stage, "stage"),
        .inMenu = false});
  verb({.name = "seq",
        .summary = "The libsnd sequencer state to the log",
        .run = logged(&diagnostics::sequencer, "seq"),
        .inMenu = false});
  verb({
      .name = "node",
      .summary = "One entity node to the log",
      .params = {DebugParam::hex("address", 0x80000000, 0x801FFFFF)},
      .run =
          [](const DebugCall &call) {
            diagnostics::describeNode(call.core, call.values.u32(0));
            call.reply = "ok: node, see the log";
          },
      .inMenu = false,
  });
  verb({
      .name = "geomblk",
      .summary = "A model's geometry block address to the log",
      .params = {DebugParam::integer("group", 0, 0xFFFF), DebugParam::integer("sub", 0, 0xFFFF)},
      .run =
          [](const DebugCall &call) {
            diagnostics::geomBlock(call.core, call.values.u32(0), call.values.u32(1));
            call.reply = "ok: geomblk, see the log";
          },
      .inMenu = false,
  });
  verb({
      .name = "invtest",
      .summary = "Fire inventory vectors through the inventory overrides",
      .params = {DebugParam::integer("type", 0, 0xFF).orAbsent(), DebugParam::integer("amount", 0, 0xFF).orAbsent()},
      .run =
          [](const DebugCall &call) {
            const auto optional = [&call](std::size_t i) {
              return call.values.has(i) ? std::optional<uint32_t>(call.values.u32(i)) : std::nullopt;
            };
            diagnostics::inventoryTest(call.core, optional(0), optional(1));
            call.reply = "ok: invtest, see the log";
          },
      .inMenu = false,
  });
  verb({
      .name = "bgm",
      .summary = "Start a song through the game's own BGM path",
      .params = {DebugParam::integer("song", 0, 0xFF)},
      .run =
          [](const DebugCall &call) {
            diagnostics::guestBgm(call.core, call.values.u32(0));
            call.reply = "ok: bgm, see the log";
          },
      .inMenu = false,
  });
  verb({.name = "bgmstop",
        .summary = "Stop the game's own BGM",
        .run = logged(&diagnostics::guestBgmStop, "bgmstop"),
        .inMenu = false});
  verb({
      .name = "seqsolo",
      .summary = "Play one libsnd sequence alone",
      .params = {DebugParam::integer("sequence", 0, diagnostics::kSequenceHandles - 1)},
      .run =
          [](const DebugCall &call) {
            diagnostics::sequenceSolo(call.core, call.values.u32(0));
            call.reply = "ok: seqsolo, see the log";
          },
      .inMenu = false,
  });
}

std::optional<psx::game::DecompImage>
DebugMenu::decompImage(Core & /*core*/, std::string_view imageName, bool resident) const {
  if (resident) {
    return psx::game::DecompImage{"tomba2", "scratch/bin/tomba2/MAIN.EXE"};
  }
  // The catalog names an overlay by its disc file stem (A00..A0L, SOP, OPN, CRD, START, DEMO, GAME).
  std::string manifest = "tomba2_";
  for (const char c : imageName) {
    manifest.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
  }
  return psx::game::DecompImage{manifest, "scratch/bin/overlays/" + std::string(imageName) + ".BIN"};
}

} // namespace tomba
