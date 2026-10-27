// diagnostics.cpp — Tomba! 2's diagnostic debug verbs; each logs what it read.
#include "debug/diagnostics.h"

#include "core.h"
#include "engine/engine.h" // game-owned entity lists + behavior registry for `ents`
#include "entry/session.h" // inv(c) Inventory
#include "guest_call.h"    // rc0/rc1/rc3 — typed runtime address dispatch of the Tomba BGM / libsnd-seq guest leaves
#include <cstdint>
#include <lucent/log.h>
#include <optional>
#include <string_view>

namespace tomba {

namespace {

void listEntitiesOf(Core *c) {
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

void describeNodeOf(Core *c, uint32_t node) {
  lucent::info("repl",
               "node {:08X}: type={:02X} state={:04X} rflag={:02X} handler={:08X}",
               node,
               c->mem_r8(node + 0xC),
               c->mem_r16(node + 0x28),
               c->mem_r8(node + 1),
               c->mem_r32(node + 0x1C));
  lucent::info("repl",
               "  pos=({},{},{}) rot=({},{},{}) model={:04X} mdata={:08X}",
               c->mem_r16s(node + 0x2E),
               c->mem_r16s(node + 0x32),
               c->mem_r16s(node + 0x36),
               c->mem_r16s(node + 0x54),
               c->mem_r16s(node + 0x56),
               c->mem_r16s(node + 0x58),
               c->mem_r16(node + 0xE) & 0x3FFF,
               c->mem_r32(node + 0x38));
  const uint32_t count = c->mem_r8(node + 8);
  lucent::info("repl", "  cmds={} next={:08X} prev={:08X}", count, c->mem_r32(node + 0x24), c->mem_r32(node + 0x20));
  for (uint32_t i = 0; i < count && i < 32; i++) {
    const uint32_t command = c->mem_r32(node + 0xC0 + i * 4);
    lucent::info("repl", "    cmd[{}]={:08X} geomblk={:08X}", i, command, command ? c->mem_r32(command + 0x40) : 0);
  }
}

} // namespace

} // namespace tomba

namespace tomba::diagnostics {

void listEntities(Core &core) {
  listEntitiesOf(&core);
}

void describeNode(Core &core, uint32_t node) {
  describeNodeOf(&core, node);
}

void stage(Core &core) {
  lucent::info("repl",
               "stage={:08X} sm48={} scene-active={:08X}",
               core.mem_r32(0x801FE00Cu),
               (int)core.mem_r16(0x801FE048u),
               core.mem_r32(0x800BE258u));
}

// The model table at *0x800ECF58: group G's table T, sub-model S's block at T + *(T + S*4 + 4).
void geomBlock(Core &core, uint32_t group, uint32_t sub) {
  const uint32_t table = core.mem_r32(0x800ECF58u + group * 4);
  lucent::info(
      "repl", "group={} sub={} T={:08X} geomblk={:08X}", group, sub, table, table + core.mem_r32(table + sub * 4 + 4));
}

void sequencer(Core &core) {
  lucent::info("repl",
               "seq open={} playmask={:04X} tickmode={} seqfn={:08X} stage={:08X}",
               core.mem_r16s(0x801054B0u),
               core.mem_r32(0x80104C28u) & 0xFFFF,
               core.mem_r8(0x800AC424u),
               core.mem_r32(0x800AC42Cu),
               core.mem_r32(0x801FE00Cu));
}

void inventoryTest(Core &core, std::optional<uint32_t> type, std::optional<uint32_t> amount) {
  Core *c = &core;
  // FUN_8004D338/D4C4/D4F4(type, amount) through the override path; with no type, a spread covering both
  // quest-ref variants, the 23..28 ring and the cap.
  constexpr int vt[] = {1, 2, 5, 10, 23, 25, 28, 40, 60, 99};
  constexpr int va[] = {1, 3, 1, 50, 1, 99, 2, 7, 1, 5};
  const int n = type ? 1 : (int)(sizeof vt / sizeof vt[0]);
  for (int i = 0; i < n; i++) {
    const uint32_t t = type ? *type : (uint32_t)vt[i];
    const uint32_t m = amount ? *amount : (type ? 1u : (uint32_t)va[i]);
    inv(c).add({.type = t, .amount = m});         // FUN_8004D338 core (via invverify gate)
    inv(c).give({.type = t, .amount = m});        // FUN_8004D4F4 give_only
    inv(c).giveAndFlag({.type = t, .amount = m}); // FUN_8004D4C4 give_and_flag
  }
  lucent::info("repl", "invtest: fired {} vector(s) through inventory overrides", n * 3);
}

void guestBgm(Core &core, uint32_t song) {
  psx::cpu::callGuestNow(core, __func__, 0x80074BF8u, song);
  lucent::info("repl", "bgm {} (song@800bed80={:04X})", song, core.mem_r16(0x800bed80));
}

void guestBgmStop(Core &core) {
  psx::cpu::callGuestNow(core, __func__, 0x80074E48u);
  lucent::info("repl", "bgmstop");
}

// SsSeqStop=0x80091AF0 every open sequence, then SsSeqPlay(h, 1, 0)=0x80090560 and SsSeqSetVol(h, 127,
// 127)=0x80091F50 for one, through the game's own sequencer; the handle is the seq access index 0..13.
void sequenceSolo(Core &core, uint32_t sequence) {
  for (uint32_t i = 0; i < kSequenceHandles; i++) {
    psx::cpu::callGuestNow(core, __func__, 0x80091AF0u, i);
  }
  psx::cpu::callGuestNow(core, __func__, 0x80090560u, sequence, 1, 0);
  psx::cpu::callGuestNow(core, __func__, 0x80091F50u, sequence, 127, 127);
  lucent::info("repl", "seqsolo {}", sequence);
}

} // namespace tomba::diagnostics
