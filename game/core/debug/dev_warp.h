#pragma once

class Core;

namespace tomba {

// Apply one complete Tomba! 2 cold-area warp. This operation is title state-machine behavior: both
// the title frame driver and the transitional framework-facing hook call this single owner.
void applyColdWarp(Core &core, int area, int sub);

} // namespace tomba
