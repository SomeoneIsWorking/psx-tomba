#include "scene/script_opcode.h"

namespace tomba::script {
namespace {

// One row per natively-owned opcode: the ids and their slugs, written down ONCE. The "unowned"
// answer is the table's own miss, generated rather than being a 63-row hand-written list of
// meanings nobody has read out of the image.
struct OwnedOpcode {
  uint16_t opcodeId;
  const char *slug;
};

constexpr OwnedOpcode kOwnedOpcodes[] = {
    {static_cast<uint16_t>(ScriptOpcode::kSceneFlagRendezvous), "scene_flag_rendezvous"},
    {static_cast<uint16_t>(ScriptOpcode::kWaitFrames), "wait_frames"},
    {static_cast<uint16_t>(ScriptOpcode::kTestSceneFlag), "test_scene_flag"},
    {static_cast<uint16_t>(ScriptOpcode::kTurnTowardTarget), "turn_toward_target"},
    {static_cast<uint16_t>(ScriptOpcode::kClaimGate), "claim_gate"},
    {static_cast<uint16_t>(ScriptOpcode::kMoveTowardScriptTarget), "move_toward_script_target"},
    {static_cast<uint16_t>(ScriptOpcode::kCallFunctionPointer), "call_function_pointer"},
    {static_cast<uint16_t>(ScriptOpcode::kUnownedOpcode), "unowned_op_3f"},
};

} // namespace

const char *scriptOpcodeName(uint16_t opcodeWord) {
  const uint16_t id = static_cast<uint16_t>(opcodeWord & kOpcodeIdMask);
  for (const OwnedOpcode &row : kOwnedOpcodes) {
    if (row.opcodeId == id) {
      return row.slug;
    }
  }
  // One of the 56 handler-table entries this repository has NOT read a meaning out of the image.
  // The caller is expected to print `opcodeWord` beside this, so the id is never lost — "we do not
  // know what this opcode does" is the finding, and hiding the id behind the name would hide it.
  return "unowned";
}

} // namespace tomba::script
