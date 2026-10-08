// class AreaFadeSequencer — guest FUN_8010957C (A0L), the per-node fade sequencer that
// Engine::fieldRun runs while sm[0x4e]==0xb.
//
// node+2 outer state (0 init, 1 running), node+3 substep 0..5, node+106 fade level 0..31,
// node+104 substep-2 delay counter.
#pragma once
#include <cstdint>
class Core;

class AreaFadeSequencer {
public:
  // Back-pointer wired once by Core's constructor.
  Core *core = nullptr;

  void step(uint32_t node);
};
