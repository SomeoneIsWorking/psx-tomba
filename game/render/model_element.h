// game/render/model_element.h — the emission element a model primitive names inside its drawing object.
//
// A model block is a count word followed by its GT3 records, then its GT4 records (FUN_800803DC,
// FUN_80146478, FUN_801401B8). A primitive's element is its list and its index in that list, so it is
// the same whatever the emitter culled before it. Element 0 stays the drawing object's own scope.
#pragma once

#include <cstdint>

namespace tomba2::render {

enum class ModelList : std::uint32_t { Gt3 = 1, Gt4 = 2 };

inline constexpr std::uint32_t modelElement(ModelList list, std::uint32_t index) {
  return (static_cast<std::uint32_t>(list) << 16) | index;
}

} // namespace tomba2::render
