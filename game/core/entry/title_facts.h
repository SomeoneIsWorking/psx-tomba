#pragma once

struct GameConfig;
struct GameHooks;

namespace tomba::title {

// The measured Tomba!2 facts, owned by the title and reachable by name.
//
// `grep -rn 'tomba::legacy' external/psxport` is empty: psxport reads `Core::cfg` and `Core::hooks`
// directly and never named the compatibility namespace these tables used to be published under. Its
// only readers were this title's TombaRuntime constructor and one test, so both take the owning
// namespace directly.
const GameConfig &measuredConfig();
const GameHooks &hooks();

} // namespace tomba::title