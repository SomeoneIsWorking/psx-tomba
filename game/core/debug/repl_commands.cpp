// repl_commands.cpp — the game-side developer commands (GameHooks::replCommand).
//
// The framework's control channel handles framework commands (memory, input, screenshots, render
// toggles); TombaRuntime::controlCommand hands every other command here. Commands that reach Tomba! 2
// classes or guest layouts live on this side so the framework names no game type or address. Returns
// true iff the command was recognised and handled.
#include "audio/music_list.h" // class MusicList — `musictest`
#include "cfg.h"
#include "core.h"
#include "engine/engine.h"  // game-owned entity lists + behavior registry for `ents`
#include "entry/game_ctx.h" // inv(c) Inventory + gctx(c)->music_list MusicList
#include "game_iface.h"
#include "guest_call.h" // rc0/rc1/rc3 — typed runtime address dispatch of the Tomba BGM / libsnd-seq guest leaves
#include <cstdint>
#include <lucent/log.h>
#include <stdio.h>
#include <string.h>

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
    cfg_logi("ents", "-- list %d head=%08X --", list, node);
    for (int guard = 0; node && guard < 400; ++guard, node = c->mem_r32(node + 0x24)) {
      const uint32_t command = c->mem_r8(node + 8) ? c->mem_r32(node + 0xC0) : 0;
      const uint32_t handler = c->mem_r32(node + 0x1C);
      const char *behavior = eng(c).behaviors.nativeName(handler);
      const int16_t nodeX = c->mem_r16s(node + 0x2E);
      const int16_t nodeZ = c->mem_r16s(node + 0x36);
      const bool isPlayer = c->mem_r32(node + 0x38) == 0 && nodeX == playerX && nodeZ == playerZ;
      if (behavior) {
        ++owned;
      }
      cfg_logi("ents",
               " %08X t=%02X ri=%02X model=%04X h=%08X pos=(%6d,%6d,%6d) rf=%u cmds=%u "
               "gb0=%08X  %s%s",
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
  cfg_logi("ents", "(%d nodes; %d native-owned, %d still-PSX)", total, owned, total - owned);
}

} // namespace

bool tomba_repl_command(Core *c, const char *cmd, const char *line) {
  unsigned a = 0;
  if (!strcmp(cmd, "ents")) {
    list_entities(c);
    return true;
  }
  if (!strcmp(cmd, "census")) {
    c->rsub.census.report("repl");
    return true;
  }
  if (!strcmp(cmd, "stage")) {
    cfg_logi("repl", "stage=%08X sm48=%d", c->mem_r32(0x801FE00Cu), (int)c->mem_r16(0x801FE048u));
    return true;
  }
  if (!strcmp(cmd, "seq")) {
    cfg_logi("repl",
             "seq open=%d playmask=%04X tickmode=%u seqfn=%08X stage=%08X",
             c->mem_r16s(0x801054B0u),
             c->mem_r32(0x80104C28u) & 0xFFFF,
             c->mem_r8(0x800AC424u),
             c->mem_r32(0x800AC42Cu),
             c->mem_r32(0x801FE00Cu));
    return true;
  }
  if (!strcmp(cmd, "invtest")) { // diagnostic: exercise the inventory subsystem with a test vector
    // invtest [type] [amt] — fire FUN_8004D338/D4C4/D4F4(type,amt) through the override path (with the
    // `invverify` gate enabled this runs the full RAM+scratchpad A/B vs the guest instruction path). With no args,
    // sweep a spread of item types/amounts covering both quest-ref variants + the 23..28 ring + the cap.
    int ty = -1, am = -1;
    sscanf(line, "%*s %d %d", &ty, &am);
    static const int vt[] = {1, 2, 5, 10, 23, 25, 28, 40, 60, 99};
    static const int va[] = {1, 3, 1, 50, 1, 99, 2, 7, 1, 5};
    int n = (ty >= 0) ? 1 : (int)(sizeof vt / sizeof vt[0]);
    for (int i = 0; i < n; i++) {
      uint32_t t = (ty >= 0) ? (uint32_t)ty : (uint32_t)vt[i];
      uint32_t m = (am >= 0) ? (uint32_t)am : (ty >= 0 ? 1u : (uint32_t)va[i]);
      inv(c).add(t, m);         // FUN_8004D338 core (via invverify gate)
      inv(c).give(t, m);        // FUN_8004D4F4 give_only
      inv(c).giveAndFlag(t, m); // FUN_8004D4C4 give_and_flag
    }
    cfg_logi("repl", "invtest: fired %d vector(s) through inventory overrides", n * 3);
    return true;
  }
  if (!strcmp(cmd, "bgm") && sscanf(line, "%*s %u", &a) == 1) {
    psx::cpu::dispatchGuestToReturn1(*c, 0x80074BF8u, a, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
    cfg_logi("repl", "bgm %u (song@800bed80=%04X)", a, c->mem_r16(0x800bed80));
    return true;
  }
  if (!strcmp(cmd, "bgmstop")) {
    psx::cpu::dispatchGuestToReturn0(*c, 0x80074E48u, psx::cpu::ExecutionBudget::currentTurn(*c), __func__);
    cfg_logi("repl", "bgmstop");
    return true;
  }
  // seqsolo <i> — stop ALL open libsnd sequences then SsSeqPlay just sequence <i> at full vol, via the
  // GAME'S OWN sequencer. Lets each area SEP sequence be rendered in isolation (the area's field theme
  // otherwise plays continuously). SsSeqStop=0x80091AF0, SsSeqPlay(h,mode,loop)=0x80090560, SsSeqSetVol
  // (h,volL,volR)=0x80091F50. handle == the seq access index (0..13).
  if (!strcmp(cmd, "seqsolo") && sscanf(line, "%*s %u", &a) == 1) {
    for (uint32_t i = 0; i < 14; i++) {
      psx::cpu::dispatchGuestToReturn1(
          *c, 0x80091AF0u, i, psx::cpu::ExecutionBudget::currentTurn(*c), __func__); // SsSeqStop(i) — silence all
    }
    psx::cpu::dispatchGuestToReturn3(
        *c, 0x80090560u, a, 1, 0, psx::cpu::ExecutionBudget::currentTurn(*c), __func__); // SsSeqPlay(a, mode=1, loop=0)
    psx::cpu::dispatchGuestToReturn3(
        *c, 0x80091F50u, a, 127, 127, psx::cpu::ExecutionBudget::currentTurn(*c), __func__); // SsSeqSetVol(a, 127, 127)
    cfg_logi("repl", "seqsolo %u", a);
    return true;
  }
  // musictest <n> — play catalogued music track <n> through the NATIVE audio engine (sound test).
  // 'musictest stop' (or n<0) stops. Bypasses the broken libsnd path entirely (engine/audio/).
  if (!strcmp(cmd, "musictest")) {
    MusicList &ml = gctx(c)->music_list; // music_list moved off Game onto the game-side TombaCtx
    char sub[32] = {0};
    int n = -1;
    if (sscanf(line, "%*s %31s", sub) == 1 && !strcmp(sub, "stop")) {
      ml.stop();
      cfg_logi("repl", "musictest stop");
    } else if (sscanf(line, "%*s %d", &n) == 1 && n >= 0) {
      int rc = ml.play(n);
      cfg_logi("repl", "musictest %d (%s) -> %s", n, ml.name(n) ? ml.name(n) : "?", rc ? "FAIL" : "ok");
    } else {
      cfg_logi("repl", "musictest: tracks 0..%d, or 'stop'", ml.count() - 1);
      for (int i = 0; i < ml.count(); i++) {
        cfg_logi("repl", "   %d: %s", i, ml.name(i));
      }
    }
    return true;
  }
  return false; // not a game command — framework prints "? <cmd>"
}
