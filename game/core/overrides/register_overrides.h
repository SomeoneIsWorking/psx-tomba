#pragma once

class Game;

// Collect title-native declarations; TombaRuntime binds them to a Core's resident image separately.
void register_engine_overrides(Game &game);
