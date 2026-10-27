// diagnostics.h — Tomba! 2's diagnostic debug verbs (ents, node, geomblk, stage, seq, invtest, bgm, seqsolo).
#pragma once

#include <cstdint>
#include <optional>

class Core;

namespace tomba::diagnostics {

// libsnd sequence access handles 0..13.
inline constexpr uint32_t kSequenceHandles = 14;

void listEntities(Core &core);
void describeNode(Core &core, uint32_t node);
void stage(Core &core);
void geomBlock(Core &core, uint32_t group, uint32_t sub);
void sequencer(Core &core);
void inventoryTest(Core &core, std::optional<uint32_t> type, std::optional<uint32_t> amount);
void guestBgm(Core &core, uint32_t song);
void guestBgmStop(Core &core);
void sequenceSolo(Core &core, uint32_t sequence);

} // namespace tomba::diagnostics
