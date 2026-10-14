#pragma once

class Core;

namespace tomba1 {

class StreamFieldTurn {
public:
  using Operation = void (*)(Core &core);

  constexpr StreamFieldTurn(Operation advanceAudio, Operation pumpDisc)
      : advanceAudio_(advanceAudio), pumpDisc_(pumpDisc) {}

  void service(Core &core) const;

private:
  Operation advanceAudio_;
  Operation pumpDisc_;
};

// One display field's audio advance and disc stream pump, called by the frame driver once per field
// it delivers.
void serviceStreamField(Core &core);

} // namespace tomba1
