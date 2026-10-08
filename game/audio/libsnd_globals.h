// game/audio/libsnd_globals.h — the LIBSND CLUSTER: the sound driver's globals, named.
//
// WHY THIS FILE EXISTS. `voiceStateFlush` — the per-frame SPU voice-state flush, 12,000 substrate
// dispatches per 6,000 replay frames — was 385 lines in which the base address appeared as the
// expression `(uint32_t)32784u << 16` eighty-seven times and every global was a bare decimal offset
// inside it: `c->mem_r32((c->r[2] + (uint32_t)23468))`. That is not a port, it is a transcript, and it
// hid the file's own bug surface: a decimal offset is a number, and nothing about the number says
// which of four neighbouring words it is or what writes it.
//
// THE BASE. `32784 << 16` is 0x80100000, and the guest's own `lui` immediate is 32784 — that is where
// the idiom comes from and it is kept as a named constant with the arithmetic shown, because the
// decimal is what makes it unreadable. It is NOT the same arithmetic as the scratchpad's 0x1F800000
// (which is a plain address in the guest's instruction stream), and a title that has both has already
// had one of them written as the other's arithmetic.
//
// WHAT IS NAMED AND WHAT IS NOT. Names come from two places, and the header says which is which:
//   * a field whose role this repository has CONFIRMED — its own comment in the driver records what
//     reads or writes it, or a gate proves it;
//   * a field whose role is NOT confirmed, named for where it is and what shape it is, with that
//     stated. `kUnnamed...` names are not a gap in this header; they are the honest answer for a
//     number nobody has read out of the image, and they are the first step to reading it.
#pragma once
#include <cstdint>

namespace tomba::audio::libsnd {

// The cluster base. Shown as the guest writes it, with the value it evaluates to, because a reader
// who cannot do the shift in their head is exactly the reader this header is for.
inline constexpr uint32_t kBase = 32784u << 16; // 0x80100000

// A cluster address from an offset, so a call site reads `addr(kFoo)` rather than a shift followed by
// an addition. The offset is the guest's own immediate; nothing is renormalised.
constexpr uint32_t addr(uint32_t offset) {
  return kBase + offset;
}

// TWO KINDS OF ADDRESS LIVE IN THE SOUND DRIVER, AND THIS HEADER KEEPS THEM APART.
//
//   * CLUSTER OFFSETS (the `u` constants below) are immediates: a call site adds them to `kBase`.
//   * ABSOLUTE ADDRESSES (the `0x...` constants) are whole guest addresses the guest forms with its
//     own `lui`/`addiu`, and are read and written as they stand.
//
// THE FIRST VERSION OF THIS HEADER HAD ONLY THE FIRST KIND, and expressed an absolute address as a
// cluster offset anyway — `70448u` for `0x800AC430`, which is not a slot in the cluster at all but a
// word in whatever overlay currently occupies that page. Every constant below therefore states, in
// its comment, the ADDRESS the decimal evaluates to, and the two kinds are in separate sections: a
// decimal that disagrees with its own comment is a defect, not a style.

// ---- ABSOLUTE: SsSeqCalled's own globals (the sequence scheduler) --------------------------------
//
// 0x80104Cxx is a page of the executable's data the scheduler reads as whole words, NOT cluster
// members; the cluster proper starts at 0x80105000. Values from the pre-rename source, which is the
// gated behaviour this header has to reproduce.

inline constexpr uint32_t kSeqReentryFlag = 0x80104C24u; // set while the tick is running
inline constexpr uint32_t kSeqActiveMask = 0x80104C28u;  // bit i set => sequence i is active
inline constexpr uint32_t kSeqPtrArray = 0x80104C30u;    // 4-byte-stride per-sequence channel tables
inline constexpr uint32_t kSeqCount = 0x801054B0u;       // s16 sequence slots
inline constexpr uint32_t kSeqChanCount = 0x801054B2u;   // s16 channels per sequence
inline constexpr uint32_t kSeqPrepFn = 0x800931C0u;      // the one-shot pre-loop call (a code address)

// ---- ABSOLUTE: the per-VBlank tick TRAMPOLINE's two dispatch slots ------------------------------
//
// THE TICK DESCRIPTOR (0x800AC424..0x800AC430) lives in the executable's own data, well below the
// 0x80100000 cluster, so `addr()` does not apply to it and the slots are published here as the
// absolute guest addresses they are.
//
// WHAT IS IN THEM, read live from the product's guest memory and out of MAIN.EXE's own data:
//
//   0x800AC42C  DAT_800ac42c  the *SsSeqCalled function pointer. The IMAGE ships it already
//                             initialised to 0x80090BD0 (SsSeqCalled itself), which is why the
//                             trampoline calls it with no null test: the slot is live code from the
//                             first frame, so the boot tick is safe to run.
//   0x800AC430  DAT_800ac430  the optional per-vblank user callback, 0 until SsSetTickMode installs
//                             one. A live read during a run shows 0x80086288 — this port's
//                             LibapiIntr::runVblankCallbacks — which is exactly the shape the
//                             trampoline has: call the user callback if installed, then call
//                             SsSeqCalled.
//
// Both are the ones `Engine::frameUpdate`'s boot guard must test (game/game_tomba2.cpp), because a
// guard that reads a DIFFERENT slot than the dispatch is not a guard (issue 0026).
inline constexpr uint32_t kUserCallback = 0x800AC430u; // DAT_800ac430 — the optional user callback slot
inline constexpr uint32_t kSeqTickFn = 0x800AC42Cu;    // DAT_800ac42c — the *SsSeqCalled function slot
inline constexpr uint32_t kTickMode = 0x800AC424u;     // DAT_800ac424 — SsSetTickMode's mode word

// ---- ABSOLUTE: the key-event scan's own words --------------------------------------------------
//
// These four are NOT cluster members and have no offset form: each is a whole address the guest
// forms itself. They are near neighbours of the cluster, which is exactly why they were mistaken
// for offsets — 0x80105D0C and 0x80105D10 are one page below the cluster and read like the
// cluster's own tail.

inline constexpr uint32_t kHardwareVoiceActive = 0x800AC3F4u;   // u32 — the SPU's own active-voice bitmask
inline constexpr uint32_t kKeyScanPitchTable = 0x801054D8u;     // s16, stride 56 — the per-voice pitch table
inline constexpr uint32_t kVolumeSnapshotScratch = 0x80105D0Cu; // u16 — channelVolumeSnapshot's dead write
inline constexpr uint32_t kKeyScanMatch = 0x80105D10u;          // u16 — the matched VOICE INDEX

// ---- ABSOLUTE: the two KON-style mask pairs -----------------------------------------------------
//
// Published as addresses rather than offsets because half their call sites add them to `kBase` and
// half use them whole; that is only correct because `kBase + 23536` and `0x80105BF0` are the same
// number, which is a trap rather than a fact. The call sites now use the address everywhere.

inline constexpr uint32_t kKonArmedMaskLo = 0x80105BF0u;    // u16 — armed-voice mask, low word
inline constexpr uint32_t kKonArmedMaskHi = 0x80105BF2u;    // u16 — armed-voice mask, high word
inline constexpr uint32_t kActiveVoiceMaskLo = 0x801054B8u; // u16 — active-voice mask, low word
inline constexpr uint32_t kActiveVoiceMaskHi = 0x801054BAu; // u16 — active-voice mask, high word

// ---- the per-VOICE table (stride 56) and the per-voice words the flush reads --------------------
//
// The table starts at `kPerVoiceTable` and each record is 56 bytes. The six words below are
// consecutive pairs inside one record: the flush reads a 16-bit value and an 8-bit value from each
// pair and packs them into the SPU register write, so the pairs are named for the register they
// become rather than for a role nobody has traced.

inline constexpr uint32_t kPerVoiceTable = 21710u;      // 0x801054CE — the stride-56 record array
inline constexpr uint32_t kPerVoiceStride = 56u;        // bytes per record
inline constexpr uint32_t kVoiceCountLimit = 15u;       // the flush's own `i < 15` loop bound
inline constexpr uint32_t kToneCountLimit = 24u;        // the tone block's own `i < 24` loop bound
inline constexpr uint32_t kPerVoicePitchLo = 21692u;    // 0x801054BC — low 16 bits of the pitch word
inline constexpr uint32_t kPerVoicePitchHi = 21694u;    // 0x801054BE — its high byte
inline constexpr uint32_t kPerVoiceLevelLo = 21696u;    // 0x801054C0
inline constexpr uint32_t kPerVoiceLevelHi = 21698u;    // 0x801054C2
inline constexpr uint32_t kPerVoiceDispatchLo = 21746u; // 0x801054F2 — s16
inline constexpr uint32_t kPerVoiceDispatchHi = 21748u; // 0x801054F4 — s16

// A per-record byte the flush CLEARS and never reads back. The flush writes 0 here as part of
// clearing a record it has just consumed, so the name says what is written, not what the byte means.
inline constexpr uint32_t kPerVoiceConsumedFlag = 21733u; // 0x801054E5

// ---- the three small loop-bound / cursor words the flush reads ----------------------------------
//
// Each is a signed byte holding a COUNT, and each is read once per pass to bound that pass's loop.
// The names say which pass, which is what the flush's control flow depends on and what a bare
// `mem_r8(base + 23788)` did not say.

inline constexpr uint32_t kVoiceCursor = 23468u;          // 0x80105BAC — incremented then masked to 4 bits
inline constexpr uint32_t kVoiceStateTable = 23472u;      // 0x80105BB0 — the 15-entry active-record array
inline constexpr uint32_t kSpuKeyScanCount = 23788u;      // 0x80105CEC — s8, bounds the key-scan pass
inline constexpr uint32_t kToneCursor = 23848u;           // 0x80105D28 — bounds the tone pass
inline constexpr uint32_t kToneBlockBase = 23048u;        // 0x80105A08 — the 24-entry tone block
inline constexpr uint32_t kUnnamedWord23072 = 23072u;     // 0x80105A20 — see the note below
inline constexpr uint32_t kUnnamedWord23080 = 23080u;     // 0x80105A28 — see the note below
inline constexpr uint32_t kUnnamedWord23464 = 23464u;     // 0x80105BA8 — see the note below
inline constexpr uint32_t kUnnamedHalfword21734 = 21734u; // 0x801054E6 — see the note below

// THE FOUR `kUnnamed` WORDS, and the one `kUnnamedHalfword`, stated once.
//
// The flush reads 23072 and dispatches through it; it reads 23080 and takes six byte-strided pointers
// off it; it reads 23464 and dispatches through it. None of the four is read anywhere else in this
// repository, and no gate has established what any of them is. Rather than give them a confident
// name, they are named for WHERE they are read and the note says so. Concretely:
//   * 23072 — READ, then dispatched through as a function pointer. The dispatch is certain; what the
//     word holds is not.
//   * 23080 — READ, then six pointers at +0/+2/+4/+6/+8/+10 are taken from it. It is the base of a
//     six-entry block of 16-bit values, and the block's shape is certain while its purpose is not.
//   * 23464 — READ, then dispatched through as a function pointer, in the same pass as 23072 and under
//     the same condition. Whether it is the same kind of word as 23072 is not established.
//   * 21734 — READ as a signed 16-bit comparison per record, and the pass it bounds is entered on a
//     DIFFERENT cursor (23072) from the 15-entry active-record pass it is not. Nothing writes it here.

} // namespace tomba::audio::libsnd
