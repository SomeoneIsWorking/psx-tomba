#pragma once

class Core;

namespace tomba1 {

// The retail 0x800163B0 initialization, after the framework applied crt0.
void runNativeBootPrefix(Core &core);

} // namespace tomba1
