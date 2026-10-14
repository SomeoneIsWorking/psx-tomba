#pragma once

#include <cstdint>

class Core;
class Game;

namespace tomba1 {

inline constexpr std::uint32_t kDmaCallbackEntry = 0x80067E84u;
// The public CdSync wrapper at 0x800648C8 forwards here unchanged. Linked libcd also calls this
// private owner directly after asynchronous commands, so both entries share one native handler.
inline constexpr std::uint32_t kCdSyncInternalEntry = 0x80065470u;

// SCUS_942.36's linked libetc VSync; Ghidra's next function begins at 0x80067D78.
inline constexpr std::uint32_t kVSyncEntry = 0x80067C30u;
// libetc's VBlank count: what VSync(-1) returns (0x80067C74), advanced by its VBlank handler 0x8006865C.
inline constexpr std::uint32_t kVSyncQueryCounter = 0x800974D4u;
// The title leaves H alone; 0x8002D784 re-asserts the same 0x220 per area.
inline constexpr std::uint32_t kSetGeomOffset = 0x80063A34u;
inline constexpr std::uint32_t kSetGeomScreen = 0x80063A54u;
inline constexpr std::uint32_t kCdRead = 0x80066A50u;
inline constexpr std::uint32_t kCdReadSync = 0x80066B30u;

// Binds the title's own CdSync/CdReady/CdControl owners and its widening SetGeomOffset.
void bindTitleLibraryOwners(Game &game);
void dmaCallbackOverride(Core *core);
void gpuTimeoutBeginOverride(Core *core);
void gpuTimeoutExpiredOverride(Core *core);

} // namespace tomba1
