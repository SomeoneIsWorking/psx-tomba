#pragma once

#include <cstdint>

class Core;

namespace tomba1 {

// CdReadyCallback() is linked at 0x80064920. Its single RAM write targets this slot, and
// StSetStream(0x80066C14) installs StCdInterrupt (0x80066CA8) through that exact setter.
inline constexpr std::uint32_t kCdReadyCallbackPointer = 0x80095FF0u;

void initializeSynchronousCd(Core &core, unsigned int resultAddress);
void cdControlOverride(Core *core);
void cdControlFOverride(Core *core);
void cdSyncOverride(Core *core);
void cdReadyOverride(Core *core);

} // namespace tomba1
