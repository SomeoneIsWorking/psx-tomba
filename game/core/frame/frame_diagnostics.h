#pragma once

#include <array>
#include <cstdint>

class Core;

namespace tomba {

// Read-only Tomba! 2 frame diagnostics. Every guest address and state-machine interpretation here is
// a title fact; the generic framework loop only invokes the title driver and reports generic results.
class FrameDiagnostics final {
public:
  void afterFrame(Core &core, uint32_t frame);

private:
  uint32_t seqLast_ = 0xffffffffu;
  uint64_t stateLastSignature_ = 0;
  std::array<uint32_t, 14> bgmReadPointers_{};
  uint32_t lastStageEntry_ = 0;
  uint32_t lastStateMachine_ = 0xffffffffu;
};

} // namespace tomba
