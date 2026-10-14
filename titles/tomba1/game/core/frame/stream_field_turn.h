#pragma once

class Core;

namespace tomba1 {

// The two per-field stream operations, in the order service() runs them.
struct StreamFieldTurn {
  using Operation = void (*)(Core &core);

  Operation advanceAudio;
  Operation pumpDisc;

  void service(Core &core) const;
};

// One display field's audio advance and disc stream pump, called by the frame driver once per field
// it delivers.
void serviceStreamField(Core &core);

} // namespace tomba1
