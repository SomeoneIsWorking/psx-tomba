// The mesh submitters as native overrides: the twelve chunk entries and the drawers that run them in order,
// each culling against the record canvas instead of the retail 320x224 literals.
#pragma once

#include "native_function.h"

#include <cstdint>
#include <functional>
#include <string_view>

namespace tomba1 {

using InstallOverride = std::function<bool(std::uint32_t, std::string_view, psx::cpu::NativeFunction)>;

// False when any entry was refused.
bool installMeshDraw(const InstallOverride &install);

} // namespace tomba1
