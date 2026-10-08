// game/ai/assembly_node.h — typed lens over the MULTI-PART ASSEMBLY node driven by guest
// FUN_8012EB54, and over the child records it animates.
//
// WHAT THE OBJECT IS: the seaside water pumps are instances of this class — a long diagonal beam with
// a curved arm, a hanging bucket and a counterweight. Established by observation (docs/findings/ai.md):
// `ents` at pad frame 6424 of replays/bugs/seesaw-weight.pad shows nodes 800FB858 (x=5562) and
// 800FB960 (x=6678), both cmds=12, matching kanban #8's independently-derived pump positions;
// 800FB960 is the node Tomba's attach pointer targets when he hangs on it. Simpler instances of the
// same class live elsewhere in the area with cmds=3/7.
//
// WHY A LENS AT ALL. The twelve leaves of this orchestrator are byte-exact binary evidence transcripts, and a
// body where every read is `c->mem_r16(c->r[17] + 96)` cannot show you a state fork — which is the
// whole difficulty of kanban #8, where a sub-part that should move does not. Naming the fields once,
// here, serves all twelve; four are ported and eight remain.
//
// SCOPE DISCIPLINE: this lens carries ONLY fields whose meaning is established, and each says where
// that comes from. An offset is not an identity — the same +0x30 is a 16.16 Y on one record and a
// plain Y on another (docs/findings/object.md) — so nothing is added here on the strength of "it looks
// like a position".
//
// port_check FOLLOWS THIS LENS: it harvests one-line write-accessors from game/**/*.h and counts a
// setter as the stores it performs (dynamic differential evidence:138-160). Keep every setter a single line with its
// mem_wN visible, or a converted body will silently stop being gate-able.
#pragma once
#include "core.h"
#include <cstdint>

// One sub-part record, reached through the +0xC0 pointer table of an assembly (AssemblyNode::
// childPtr) OR of the companion the assembly spawns (AssemblyCompanion::kPartTable) — it is the
// SAME record type in both, namely the CHILD role of the generic scene-node struct that
// game/render/node_xform.cpp's `Node` lens documents (sentinel 0x06, childEuler 0x08, frameMatrix
// 0x18, framePos 0x2C, childScale 0x38). Only the fields the ported leaves touch are named; this
// record is larger and the rest is not yet established.
class AssemblyChild {
public:
  AssemblyChild(Core *c, uint32_t at) : mCore(c), mAt(at) {}
  uint32_t addr() const {
    return mAt;
  }

  // +0x3E is the sub-part's own state/flags byte. Low two bits are a busy/idle state the arm path
  // requires to be zero before it will start a part; bit1 is set on the commanded part itself.
  uint32_t stateFlags() const {
    return mCore->mem_r8(mAt + 0x3Eu);
  }
  bool idle() const {
    return (stateFlags() & 3u) == 0;
  }
  void setStateFlags(uint32_t v) const {
    mCore->mem_w8(mAt + 0x3Eu, (uint8_t)v);
  }

  // +0x0C is the oscillator accumulator FUN_80130D5C drives; the arm path reads it as the fallback
  // angle when the node's own angle selector does not match the pending command. (Same bytes
  // node_xform.cpp's CHILD role calls childEulerZ — an angle either way, two vocabularies for one
  // field, not a contradiction.)
  uint32_t accumulator() const {
    return mCore->mem_r16(mAt + 0x0Cu);
  }

  // --- the sub-part's own transform slots, added when 0x801389C8 was ported -----------------------
  // frameMatrix (+0x18, 5-word packed GTE MATRIX): the part's composed local/frame matrix. It is
  //   the OUTPUT of NodeXform::propagate/propagateAxis (game/render/node_xform.cpp, which names the
  //   same +0x18 frameMatrixPtr) and the in/out buffer 0x801389C8 hands to MulMatrix0 (0x80084250)
  //   when it folds the part's own scale in.
  uint32_t frameMatrixPtr() const {
    return mAt + 0x18u;
  }
  // scale (+0x38/+0x3A/+0x3C, 3x s16): the part's per-axis scale in GTE 4.12 fixed point, so 0x1000
  //   is 1.0. Established from the WRITER, not from this reader: the companion's state-0 init
  //   overlay guest 0x80136F08 allocates every rig part through the generic child spawner 0x8007AAE8,
  //   stores it into its own node+0xC0 slot, and seeds exactly this field group — sentinel +6 =
  //   slot-1, euler +8/+10/+12 from the role's blueprint, then 0x1000 into all three of +0x38,
  //   +0x3A and +0x3C. node_xform.cpp's independently-RE'd Node lens names the same three offsets
  //   childScaleX/Y/Z and feeds them to the same diagonal-matrix seeder (0x800517BC).
  int32_t scaleX() const {
    return mCore->mem_r16s(mAt + 0x38u);
  }
  int32_t scaleY() const {
    return mCore->mem_r16s(mAt + 0x3Au);
  }
  int32_t scaleZ() const {
    return mCore->mem_r16s(mAt + 0x3Cu);
  }

private:
  Core *mCore;
  uint32_t mAt;
};

class AssemblyNode {
public:
  AssemblyNode(Core *c, uint32_t at) : mCore(c), mAt(at) {}

  uint32_t addr() const {
    return mAt;
  }

  // --- walk/orchestrator header (shared with every type-04 behaviour node) -----------------------
  uint8_t state() const {
    return mCore->mem_r8(mAt + 0x04u);
  } // node[4], the outer state
  uint8_t subState() const {
    return mCore->mem_r8(mAt + 0x05u);
  } // node[5], the sub-state
  uint8_t partCount() const {
    return mCore->mem_r8(mAt + 0x08u);
  } // `cmds` in the ents dump

  // --- the child-record table this class animates ------------------------------------------------
  // node+0xC0 is an array of pointers, one per sub-part. NodeXform::propagate walks the SAME table
  // (game/render/node_xform.cpp), which is what ties these sub-parts to their transforms.
  static constexpr uint32_t kChildTable = 0xC0u;
  uint32_t childPtr(int slot) const {
    return mCore->mem_r32(mAt + kChildTable + (uint32_t)slot * 4u);
  }

  // --- the assembly's configuration word at +0x60 -------------------------------------------------
  // A bitfield the leaves re-read on EVERY use rather than caching, because the sub-part tick can
  // change it mid-loop. Only the three bits the ported leaves actually test are named; the rest of
  // the word is not yet understood and is deliberately not guessed at.
  //
  // Read UNSIGNED (the guest uses lhu at every site).
  uint32_t configWord() const {
    return mCore->mem_r16(mAt + 0x60u);
  }
  bool hasOscillatingParts() const {
    return (configWord() & 0x4u) != 0;
  } // bit2 — gates the tick entirely
  bool oscillatorPairMode() const {
    return (configWord() & 0x2u) != 0;
  } // bit1 — two driven slots, and
    // biases each slot index by +1

  // --- fields the ARM-PENDING-PAIR leaf (FUN_80131134) works over --------------------------------
  // Meanings below are read off that body's use, not guessed: each is described by what the code
  // DOES with it, and anything whose role is not settled says so.
  uint32_t roleByte() const {
    return u8(0x03u);
  } // < 2 selects the two "master" assemblies
  uint32_t pendingCommand() const {
    return u16(0x7Au) & 3u;
  } // 2-bit command; 0 = nothing pending
  bool pendingBit2() const {
    return (u16(0x7Au) & 4u) != 0;
  } // extra flag on the same word
  uint32_t modeByte() const {
    return u8(0x5Eu);
  } // bit1 selects the angle source below
  int32_t angleSelector() const {
    return s16(0x6Cu);
  } // compared against the pending command
  uint32_t angleParam() const {
    return u16(0x6Eu);
  } // used masked to 12 bits (a PSX angle)
  uint32_t armDuration() const {
    return u16(0x72u);
  } // set from the command, then optionally +2
  void setArmDuration(uint32_t v) const {
    mCore->mem_w16(mAt + 0x72u, (uint16_t)v);
  }

  // --- the two fields the assembly PUBLISHES to its spawned companion (game/ai/assembly_companion.h) ---
  // Added when 0x80138A64 was ported: that leaf reads both across the child->parent link, so their
  // meaning is pinned by what the assembly's own leaves write, not by the reader.
  //
  // strokePhase (+0x76, s16): how far through its stroke the assembly is, as a 0/1/2 tag rather than
  //   an angle. overlay guest 0x8012F5B4 (the sub-state leaf that drives the beam) sets it to 1 when the
  //   swing crosses into the negative direction, and promotes 1 -> 2 on the frame the beam is clamped
  //   at the far limit (-2560, where it also flips the step at +0x4E to +512); it stores 0 on every
  //   path that parks the beam at rest. The init leaf overlay guest 0x8012ED84 zeroes it alongside
  //   angleParam/armDuration/pendingCommand. So 1 = "first half-stroke reached", which is the single
  //   value the spawned companion arms on.
  int32_t strokePhase() const {
    return s16(0x76u);
  }
  // strokeFlags (+0x78, s16 here): a flag word on the same pair — 8012F5B4 tests bit2 of it as an
  //   inhibit before it will advance the swing, and clears it wherever it clears strokePhase. Only
  //   its all-clear/not-clear sense is established; individual bits beyond bit2 are not, so nothing
  //   here names them.
  int32_t strokeFlags() const {
    return s16(0x78u);
  }

protected:
  int32_t s16(uint32_t off) const {
    return mCore->mem_r16s(mAt + off);
  }
  uint32_t u16(uint32_t off) const {
    return mCore->mem_r16(mAt + off);
  }
  uint32_t u8(uint32_t off) const {
    return mCore->mem_r8(mAt + off);
  }

  Core *mCore;
  uint32_t mAt;
};
