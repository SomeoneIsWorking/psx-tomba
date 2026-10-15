// repl_commands.cpp — Tomba! 2's developer commands, reached through Runtime::titleCommand.
#include "debug/repl_commands.h"

#include "audio/music_list.h" // class MusicList — `musictest`
#include "core.h"
#include "debug/dev_args.h"
#include "engine/engine.h" // game-owned entity lists + behavior registry for `ents`
#include "entry/session.h" // inv(c) Inventory + sessionOf(c).music_list MusicList
#include "guest_call.h"    // rc0/rc1/rc3 — typed runtime address dispatch of the Tomba BGM / libsnd-seq guest leaves
#include <cstdint>
#include <lucent/log.h>
#include <string_view>

namespace tomba {

namespace {

void list_entities(Core *c) {
  // Tomba! 2 objects are nodes in three doubly-linked lists (next @ +0x24). This layout and every
  // address below are title facts, so the command lives here rather than in psxport's REPL parser.
  const int16_t playerX = (int16_t)(c->mem_r32(0x800E7EACu) >> 16);
  const int16_t playerZ = (int16_t)(c->mem_r32(0x800E7EB4u) >> 16);
  const uint32_t heads[3] = {0x800FB168u, 0x800F2624u, 0x800F2738u};
  int total = 0;
  int owned = 0;
  for (int list = 0; list < 3; ++list) {
    uint32_t node = c->mem_r32(heads[list]);
    lucent::info("ents", "-- list {} head={:08X} --", list, node);
    for (int guard = 0; node && guard < 400; ++guard, node = c->mem_r32(node + 0x24)) {
      const uint32_t command = c->mem_r8(node + 8) ? c->mem_r32(node + 0xC0) : 0;
      const uint32_t handler = c->mem_r32(node + 0x1C);
      const char *behavior = eng(c).behaviors.nativeName(handler);
      const int16_t nodeX = (int16_t)c->mem_r16(node + 0x2E);
      const int16_t nodeZ = (int16_t)c->mem_r16(node + 0x36);
      const bool isPlayer = c->mem_r32(node + 0x38) == 0 && nodeX == playerX && nodeZ == playerZ;
      if (behavior) {
        ++owned;
      }
      lucent::info(
          "ents",
          " {:08X} t={:02X} ri={:02X} model={:04X} h={:08X} pos=({:6},{:6},{:6}) rf={} cmds={} gb0={:08X}  {}{}",
          node,
          c->mem_r8(node + 0xC),
          c->mem_r8(node + 0xB),
          c->mem_r16(node + 0xE) & 0x3FFF,
          handler,
          c->mem_r16s(node + 0x2E),
          c->mem_r16s(node + 0x32),
          nodeZ,
          c->mem_r8(node + 1),
          c->mem_r8(node + 8),
          command ? c->mem_r32(command + 0x40) : 0,
          behavior ? behavior : "PSX",
          isPlayer ? "  <== PLAYER" : "");
      ++total;
    }
  }
  lucent::info("ents", "({} nodes; {} native-owned, {} still-PSX)", total, owned, total - owned);
}

} // namespace

} // namespace tomba

namespace tomba {

bool replCommand(Core &core, const char *line) {
  Core *c = &core;
  const auto words = DevArgs::words(line);
  if (words.empty()) {
    return false;
  }
  const std::string_view cmd = words[0];
  uint32_t a = 0;
  const bool hasNumber = words.size() >= 2 && DevArgs::decimal(words[1], a);
  if (cmd == "ents") {
    list_entities(c);
    return true;
  }
  if (cmd == "census") {
    c->rsub.census.report("repl");
    return true;
  }
  if (cmd == "stage") {
    lucent::info("repl", "stage={:08X} sm48={}", c->mem_r32(0x801FE00Cu), (int)c->mem_r16(0x801FE048u));
    return true;
  }
  if (cmd == "seq") {
    lucent::info("repl",
                 "seq open={} playmask={:04X} tickmode={} seqfn={:08X} stage={:08X}",
                 c->mem_r16s(0x801054B0u),
                 c->mem_r32(0x80104C28u) & 0xFFFF,
                 c->mem_r8(0x800AC424u),
                 c->mem_r32(0x800AC42Cu),
                 c->mem_r32(0x801FE00Cu));
    return true;
  }
  if (cmd == "invtest") { // diagnostic: exercise the inventory subsystem with a test vector
    // invtest [type] [amt] — fire FUN_8004D338/D4C4/D4F4(type,amt) through the override path (with the
    // `invverify` gate enabled this runs the full RAM+scratchpad A/B vs the guest instruction path). With no args,
    // sweep a spread of item types/amounts covering both quest-ref variants + the 23..28 ring + the cap.
    int ty = -1;
    int am = -1;
    uint32_t word = 0;
    if (words.size() >= 2 && DevArgs::decimal(words[1], word)) {
      ty = static_cast<int>(word);
    }
    if (words.size() >= 3 && DevArgs::decimal(words[2], word)) {
      am = static_cast<int>(word);
    }
    constexpr int vt[] = {1, 2, 5, 10, 23, 25, 28, 40, 60, 99};
    constexpr int va[] = {1, 3, 1, 50, 1, 99, 2, 7, 1, 5};
    int n = (ty >= 0) ? 1 : (int)(sizeof vt / sizeof vt[0]);
    for (int i = 0; i < n; i++) {
      uint32_t t = (ty >= 0) ? (uint32_t)ty : (uint32_t)vt[i];
      uint32_t m = (am >= 0) ? (uint32_t)am : (ty >= 0 ? 1u : (uint32_t)va[i]);
      inv(c).add({.type = t, .amount = m});         // FUN_8004D338 core (via invverify gate)
      inv(c).give({.type = t, .amount = m});        // FUN_8004D4F4 give_only
      inv(c).giveAndFlag({.type = t, .amount = m}); // FUN_8004D4C4 give_and_flag
    }
    lucent::info("repl", "invtest: fired {} vector(s) through inventory overrides", n * 3);
    return true;
  }
  if (cmd == "bgm" && hasNumber) {
    psx::cpu::callGuestNow(*c, __func__, 0x80074BF8u, a);
    lucent::info("repl", "bgm {} (song@800bed80={:04X})", a, c->mem_r16(0x800bed80));
    return true;
  }
  if (cmd == "bgmstop") {
    psx::cpu::dispatchGuestToReturn0(*c, 0x80074E48u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
    lucent::info("repl", "bgmstop");
    return true;
  }
  // seqsolo <i> — stop ALL open libsnd sequences then SsSeqPlay just sequence <i> at full vol, via the
  // GAME'S OWN sequencer. Lets each area SEP sequence be rendered in isolation (the area's field theme
  // otherwise plays continuously). SsSeqStop=0x80091AF0, SsSeqPlay(h,mode,loop)=0x80090560, SsSeqSetVol
  // (h,volL,volR)=0x80091F50. handle == the seq access index (0..13).
  if (cmd == "seqsolo" && hasNumber) {
    for (uint32_t i = 0; i < 14; i++) {
      psx::cpu::callGuestNow(*c, __func__, 0x80091AF0u, i); // SsSeqStop(i) — silence all
    }
    psx::cpu::callGuestNow(*c, __func__, 0x80090560u, a, 1, 0);     // SsSeqPlay(a, mode=1, loop=0)
    psx::cpu::callGuestNow(*c, __func__, 0x80091F50u, a, 127, 127); // SsSeqSetVol(a, 127, 127)
    lucent::info("repl", "seqsolo {}", a);
    return true;
  }
  // musictest <n> — play catalogued music track <n> through the NATIVE audio engine (sound test).
  // 'musictest stop' (or n<0) stops. Bypasses the broken libsnd path entirely (engine/audio/).
  if (cmd == "musictest") {
    MusicList &ml = sessionOf(c).music_list; // music_list moved off Game onto the game-side Session
    if (words.size() >= 2 && words[1] == "stop") {
      ml.stop();
      lucent::info("repl", "musictest stop");
    } else if (hasNumber && a <= 0x7FFFFFFFu) {
      const int n = static_cast<int>(a);
      int rc = ml.play(n);
      lucent::info("repl", "musictest {} ({}) -> {}", n, ml.name(n) ? ml.name(n) : "?", rc ? "FAIL" : "ok");
    } else {
      lucent::info("repl", "musictest: tracks 0..{}, or 'stop'", ml.count() - 1);
      for (int i = 0; i < ml.count(); i++) {
        lucent::info("repl", "   {}: {}", i, ml.name(i));
      }
    }
    return true;
  }
  return false; // not a game command — framework prints "? <cmd>"
}

} // namespace tomba
