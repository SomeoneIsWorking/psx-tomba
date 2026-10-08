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

void registerStreamFieldTurn(Core &core);
void serviceStreamHostTurn(Core *core);

} // namespace tomba1
