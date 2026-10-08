// game/scene/scene_flags.h — the SCENE-FLAG BYTE ARRAY, defined in exactly one place.
//
// WHY THIS HEADER EXISTS. Two subsystems share this array and each used to carry its own copy of
// the base address: ScriptInterp (op04 posts/awaits a byte, op06 tests one) and SceneEvents
// (applyFlagOp sets/ORs/ANDs one). One of the two copies was 0x20 low, so every script-driven
// "set scene flag" landed in the wrong byte while every script-driven "test/await" read the right
// one. Scripts that hand a cutscene back and forth through this array then deadlocked — the poster
// wrote a byte nobody was watching (kanban #60, docs/findings/scene.md). A shared address with two
// definitions is a bug waiting to happen; this header is the one definition.
//
// The guest builds the base as `lui 0x800C; addiu -1936` in every function that touches it —
// 32780<<16 - 1936 = 0x800BF870 — and the flag bytes sit at a fixed +324 into that block. Both
// numbers are taken straight from guest 0x80042448 / guest 0x800420AC / guest 0x8004201C.
#pragma once
#include <cstdint>

namespace scene_flags {

// The area/scene global control block. The SAME anchor the area-mode byte, the inventory and the
// save/state block are all indexed from — see docs/engine_re.md.
constexpr uint32_t kBlockBase = 0x800BF870u; // == (32780u << 16) - 1936

// The flag byte array within that block. Indexed by a SIGNED script argument, so a negative index
// deliberately reaches below the array — preserved rather than "cleaned up".
constexpr uint32_t kFlagTable = kBlockBase + 324u;

inline uint32_t flagAddr(int32_t index) {
  return kFlagTable + (uint32_t)index;
}

} // namespace scene_flags
