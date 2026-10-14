#pragma once

class Game;

// Collect title-native declarations; TombaRuntime binds them to a Core's resident image separately.
namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba {

void register_engine_overrides(Game &game, native::NativeOverrideCatalog &natives);

} // namespace tomba
