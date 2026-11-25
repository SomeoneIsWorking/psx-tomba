#include "audio/song_switch.h"

#include "audio/music_coord.h"
#include "audio/sequencer_record.h"
#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "entry/session.h"
#include "guest_abi.h"
#include "guest_call.h"

namespace tomba {

namespace {

constexpr std::uint32_t kSongs = 14;
constexpr std::uint32_t kSongTable = 0x800BE368u; // per song: s16 sequence, s16 slot count at +4 (u8 voices)
constexpr std::uint32_t kSlotCount = 0x800BED78u; // s32, the current song's slots
constexpr std::uint32_t kSlots = 0x800BE238u;     // 12 bytes each
constexpr std::uint32_t kSlotStride = 12u;
constexpr std::uint32_t kVolumeSfx = 0x800FB164u;
constexpr std::uint32_t kVolumeMusic = 0x800FB165u;
constexpr std::uint32_t kArea = 0x800BF870u;
constexpr std::uint8_t kAreaUsingSfxVolume = 0x15;
constexpr std::uint16_t kNone = 0xFFFFu;
// libsnd.
constexpr std::uint32_t kReserveVoices = 0x800963A0u;
constexpr std::uint32_t kSeqSetVol = 0x80091F50u;
constexpr std::uint32_t kSeqPlay = 0x80090560u;
constexpr std::uint32_t kSeqStop = 0x80091AF0u;
constexpr std::uint32_t kSndPause = 0x80091050u;  // keys the sequence off, play mode 0
constexpr std::uint32_t kSndReplay = 0x80091910u; // play mode 1
constexpr std::uint32_t kPlaying = 0x1u;          // record flags: the tick reads the sequence (set by 0x80091810)

bool isTone(std::uint32_t song) {
  return song - 4u < 4u || song == 10u || song == 13u;
}

std::int16_t sequenceOf(Core &c, std::uint32_t song) {
  return static_cast<std::int16_t>(c.mem_r16(kSongTable + (song * 8u)));
}

audio::record::ChannelRecord recordOf(Core &c, std::int16_t sequence) {
  return {&c, c.mem_r32(audio::record::seqPtrSlot(static_cast<std::uint16_t>(sequence))) + audio::record::chStride(0)};
}

void pause(Core &c, std::int16_t sequence) {
  psx::cpu::callGuestNow(c, "SongSwitch::pause", kSndPause, sequence, 0);
  recordOf(c, sequence).clearFlagBits(kPlaying);
}

void replay(Core &c, std::int16_t sequence) {
  audio::record::ChannelRecord record = recordOf(c, sequence);
  record.setFlags(record.flags() | kPlaying);
  psx::cpu::callGuestNow(c, "SongSwitch::replay", kSndReplay, sequence, 0);
}

constexpr GuestFrameSpill kPlaySpills[] = {{31, 0x1C}, {18, 0x18}, {17, 0x14}, {16, 0x10}};
constexpr GuestFrameSpill kStopSpills[] = {{31, 0x10}};

void ovPlay(Core *c) {
  eng(c).songSwitch.play(c->r[4]);
}

void ovStop(Core *c) {
  eng(c).songSwitch.stop();
}

} // namespace

void SongSwitch::play(std::uint32_t request) {
  Core &c = *core;
  GuestFrame<0x20, 4> frame(&c, kPlaySpills);
  const std::uint32_t song = request & 0x7Fu;
  if (song >= kSongs) {
    return;
  }
  const std::uint32_t current = c.mem_r16(kCurrentSong);
  bool parking = false;
  switch (song) {
  case 0:
  case 1:
  case 11:
  case 12:
    if (current - 2u < 2u) {
      return;
    }
    [[fallthrough]];
  case 2:
  case 3:
    if (isTone(current)) {
      c.mem_w8(kResumeSong, static_cast<std::uint8_t>(current | 0x80u));
      parking = true;
    }
    break;
  default:
    if (current - 1u < 3u) {
      c.mem_w8(kResumeSong, static_cast<std::uint8_t>(request));
      return;
    }
    break;
  }

  const bool resuming = (request & 0x80u) != 0 && paused_ == song;
  if (paused_ && !resuming && (*paused_ == song || (c.mem_r8(kResumeSong) & 0x7Fu) != *paused_)) {
    psx::cpu::callGuestNow(c, __func__, kSeqStop, sequenceOf(c, *paused_)); // abandoned
    paused_.reset();
  }
  release(parking);
  if (parking) {
    paused_ = static_cast<std::uint16_t>(current);
  }

  c.mem_w16(kCurrentSong, static_cast<std::uint16_t>(song));
  const std::uint32_t entry = kSongTable + (song * 8u);
  psx::cpu::callGuestNow(c, __func__, kReserveVoices, c.mem_r8(entry + 4u));
  const bool music = song < 4u || (song >= 11u && song < 13u);
  const std::uint32_t level = c.mem_r8((music && c.mem_r8(kArea) != kAreaUsingSfxVolume) ? kVolumeMusic : kVolumeSfx);
  const auto volume = static_cast<std::int16_t>((level * 0x7Fu) / 9u);
  const std::int16_t sequence = sequenceOf(c, song);
  psx::cpu::callGuestNow(c, __func__, kSeqSetVol, sequence, volume, volume);
  if (resuming) {
    replay(c, sequence);
    paused_.reset();
  } else {
    psx::cpu::callGuestNow(c, __func__, kSeqPlay, sequence, 1, (request & 0x80u) == 0 ? 1 : 0);
  }
  const auto slots = static_cast<std::int16_t>(c.mem_r16(entry + 4u));
  c.mem_w32(kSlotCount, static_cast<std::uint32_t>(static_cast<std::int32_t>(slots)));
  for (std::int32_t i = 0; i < slots; ++i) {
    c.mem_w32(kSlots + (static_cast<std::uint32_t>(i) * kSlotStride), 0xFFFFFFFFu);
  }
}

void SongSwitch::stop() {
  GuestFrame<0x18, 1> frame(core, kStopSpills);
  release(false);
}

void SongSwitch::release(bool keepPaused) {
  Core &c = *core;
  const std::uint16_t current = c.mem_r16(kCurrentSong);
  if (current == kNone) {
    return;
  }
  if (keepPaused) {
    pause(c, sequenceOf(c, current));
  } else {
    psx::cpu::callGuestNow(c, __func__, kSeqStop, sequenceOf(c, current));
  }
  const auto count = static_cast<std::int32_t>(c.mem_r32(kSlotCount));
  for (std::int32_t i = 0; i < count; ++i) {
    const std::uint32_t slot = kSlots + (static_cast<std::uint32_t>(i) * kSlotStride);
    c.mem_w8(slot, 0);
    c.mem_w8(slot + 1u, c.mem_r8(slot + 1u) & 0xC0u);
  }
  c.mem_w32(kSlotCount, 0);
  c.mem_w16(kCurrentSong, kNone);
  psx::cpu::callGuestNow(c, __func__, kReserveVoices, 0);
}

void SongSwitch::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declare(0x80074BF8u, "SongSwitch::play", ovPlay);
  natives.declare(0x80074E48u, "SongSwitch::stop", ovStop);
}

} // namespace tomba
