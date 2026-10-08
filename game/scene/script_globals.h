// game/scene/script_globals.h — the FIXED guest addresses the cutscene script interpreter touches
// that are NOT fields of the driven object.
//
// `ScriptObject` (script_object.h) owns the object's own block. These are the other three kinds of
// address the interpreter reaches, and they were previously bare hex scattered through
// `script_interp.cpp` with their provenance in per-call-site comments:
//
//   * scratchpad slots  — 0x1F800000-relative, the guest's 1 KB scratch block, holding the
//                         per-frame look anchors and the shared secondary-actor pointer.
//   * one resident .bss byte  — the claim gate op34 contends on.
//   * a global / scratchpad PAIR — the status byte op-mirror copies between, and nothing else.
//
// The "what is NOT known" notes live here, once, instead of being repeated (and drifting) at each
// use site. A named "we do not know what this is" is the first step to finding out; an invented
// confident name is not.
#pragma once
#include <cstdint>

namespace tomba::scene::script_globals {

// The PSX scratchpad: 1 KB at 0x1F800000, mirroring the top 1 KB of the PSX's 4 MB of D-scratch.
// It is a plain address in the guest's instruction stream, NOT a `n << 16` cluster base the way the
// libsnd globals at 0x80100000 are (that one is 32784<<16, which is a different arithmetic entirely).
inline constexpr uint32_t kScratchpadBase = 0x1F800000u;
inline constexpr uint32_t kScratchpadBytes = 1024u;

// op31's per-frame look anchors, read by its mode 1 and mode 2 arms. WHICH scene concept they
// anchor is not established: they are read as a signed 16-bit X and a signed 16-bit Z and nothing
// writes them in this repository, so the names say where they are read, not what they mean.
inline constexpr uint32_t kLookAnchorZ = kScratchpadBase + 0x160u;
inline constexpr uint32_t kLookAnchorX = kScratchpadBase + 0x164u;

// op31's acted-upon-actor selector, and the same slot as the "global actor record" the
// kind-selector query follows. ONE guest word with TWO behaviours depending on which opcode reads
// it: op31 takes it as the actor whose facing it turns, `matchesActiveByKind` as a record it reads
// a match byte out of. Both readings are recorded here rather than leaving two names for one word
// in two files, which is how a 0x60 transcription slip survived a RE pass.
inline constexpr uint32_t kSecondaryActor = kScratchpadBase + 0x214u;

// op36's event-pulse repeat gate: a shared mask of "already handled this frame" bits. A set of bits
// turns the per-move sound pulse OFF. Who sets the mask each frame is NOT established here.
inline constexpr uint32_t kEventHandledMask = kScratchpadBase + 0x17Cu;

// op34's contended gate. Provenance, because this address was WRONG ONCE: the wide-RE draft had
// 0x800BF86F, which is 0x60 low and turned out to be an unrelated table's address. It was
// recomputed from the guest's own constant arithmetic (32780<<16 + -2040 + 7) and confirmed
// independently against the poll path's -2033. It is NOT part of `scene_flags::kFlagTable`'s
// family, and neither the producer nor the consumer of this byte has been traced.
inline constexpr uint32_t kClaimGate = 0x800BF80Fu;

// The status byte pair the mirror leaf copies between. The source is a main-RAM global and the
// destination a scratchpad byte. WHAT the byte means is not established: the leaf writes it and
// returns 1, and nothing in this repository reads the destination back.
inline constexpr uint32_t kStatusByteSource = 0x800E7EAau;              // main-RAM global
inline constexpr uint32_t kStatusByteMirror = kScratchpadBase + 0x207u; // scratchpad destination

} // namespace tomba::scene::script_globals
