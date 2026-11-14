// class MusicCoord — the dialog and area-music coordination on the shared XA stream (a PC mod for instant CD
// reads: the looping area music stays held while a dialog tone plays), and the voice-channel mixer FUN_80075824.
#pragma once
#include <stdint.h>
class Core;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba {

// FUN_80074BF8's BGM state: the playing song (s16, -1 none) and the song it resumes after the current one
// (u8, 0 none, bit 7 when the resumed song was interrupted).
inline constexpr uint32_t kCurrentSong = 0x800BED80u;
inline constexpr uint32_t kResumeSong = 0x800BE22Au;

class MusicCoord {
public:
  Core *core = nullptr;

  // Last-logged song, XA-active and XA-loop, so tick() logs its [coord] line only on a change.
  uint32_t mPrev = 0xDEAD;
  int mPa = -1, mPl = -1;

  // A dialog tone (songs 4..7) is the current song or the one FUN_80074BF8 parked for resumption while a
  // menu song plays over it.
  bool dialogToneActive();

  // tick(): once-per-frame update (called from native_step_frame). Enforces "dialogs stop
  //   the ingame music" — stops a looping ingame-music clip while a dialog tone is up; resumes
  //   the remembered clip once the dialog ends and the XA stream is free (no voice playing).
  void tick();

  // voiceMixTick(voice_base): per-frame VOICE-CHANNEL VOLUME MIXER — port of FUN_80075824
  //   (RE'd via ghidra on 2026-07-03; docs/findings/audio.md). Ramps the voice channel's current
  //   volume (u16 at +0x2C) toward its target (u16 at +0x2A) by ±0x100/frame (±0x400 when
  //   scratchpad 0x1F800137 == 2), applies a boost when the scratchpad "boost" flag (0x1F80027E)
  //   is nonzero, folds in the ADSR-like second-stage smoother (u16 at +0x30 → +0x2E), and writes
  //   the packed 16-bit volume to voice[+0x10] and voice[+0x12], the pan words voice[+0x04] and
  //   voice[+0x06] to 0x3FFF, and OR's 0xC0 into voice[+0x00] (dirty flag).
  //   Special short-circuits:
  //     scratchpad[+0x19A] != 2 → shortcut: vol = base * 0x47FF >> 15 (boot / silence state).
  //     scratchpad[+0x137] == 1 → dialog mode: vol picks a full/scaled path from DAT_800BE0E4
  //     flags, and OR's 0x3 (extra flags) into voice[+0x00].
  //   Also drops the fade to the "far" level (writes DAT_800BE222 = 0x47FF and pings the SPU
  //   queue helper 0x800750D8) if the running vol falls below 0x10 while voice[+0x33] is armed.
  //   Callable on any voice base; the field frame calls it with voice_base = 0x800BE1F8 (the
  //   ambient/XA channel), per AreaSlots::updateTail (game/world/area_slots.cpp).
  void voiceMixTick(uint32_t voice_base);

  // setGain2(val): FUN_80075D24 — sets the ambient-voice second-stage gain the voiceMixTick
  //   smoother chases (0x800BE1F8+0x2E, "g2_target" in voiceMixTick's comment). val < 0: an
  //   INSTANT snap — writes -val into BOTH the target (+0x2E) AND the current (+0x30), so the
  //   smoother has nothing left to chase (no fade). val >= 0: a RAMPED set — clamps val to
  //   [0,0x1FFF] and writes only the target (+0x2E); voiceMixTick's `g2 += (target-g2)>>3` then
  //   eases toward it over subsequent frames. RE'd via Ghidra headless (scratch/decomp/cluster1.c:
  //   FUN_80075d24).
  void setGain2(int32_t val);

  // registerOverrides(): wires FUN_80075D24 into the global override registry (no static
  // `a direct guest-address call` call site exists in the guest output — only reached via indirect
  // typed runtime address dispatch).
  void registerOverrides(native::NativeOverrideCatalog &natives);
};

} // namespace tomba
