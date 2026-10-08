#pragma once

#include <cstdint>

struct PlatformHlePlan;
struct Core;

namespace tomba1 {

inline constexpr std::uint32_t kDmaCallbackEntry = 0x80067E84u;
// The public CdSync wrapper at 0x800648C8 forwards here unchanged. Linked libcd also calls this
// private owner directly after asynchronous commands, so both entries share one native handler.
inline constexpr std::uint32_t kCdSyncInternalEntry = 0x80065470u;

const PlatformHlePlan &platformHlePlan();
void dmaCallbackOverride(Core *core);
void gpuTimeoutBeginOverride(Core *core);
void gpuTimeoutExpiredOverride(Core *core);

} // namespace tomba1
