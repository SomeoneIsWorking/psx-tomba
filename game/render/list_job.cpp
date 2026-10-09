// game/render/list_job.cpp — ListJobWriter and ListJob. See list_job.h.
#include "list_job.h"

#include "gte_control.h"
#include "guest_ordering_table.h"

#include "state_bytes.h"

#include <optional>

namespace tomba2::render {
namespace {

constexpr std::uint32_t kRangeWords = 4; // address, size, blend, modulus

} // namespace

ListCall ListCall::fromRegisters(const Core &core) {
  return ListCall{core.r[4], core.r[5], core.r[6], core.r[7], core.r[29]};
}

ListJobWriter::ListJobWriter(Core &core, std::uint32_t variant, const ListCall &call, std::uint32_t arenaBytes) {
  header_.variant = variant;
  header_.call = call;
  header_.pool = PacketPool(core).cursor();
  header_.arenaBytes = arenaBytes;
  const std::optional<psx::present::OtSlot> slot = core.otTables.slotOf(call.ot);
  named_ = slot.has_value();
  if (named_) {
    header_.table = slot->table;
    header_.slot = slot->index;
  }
  header_.control = psx::present::readGteControl();
}

void ListJobWriter::input(
    Core &core, std::uint32_t address, std::uint32_t size, InputBlend blend, std::uint32_t modulus) {
  ranges_.push_back(address);
  ranges_.push_back(size);
  ranges_.push_back(static_cast<std::uint32_t>(blend));
  ranges_.push_back(modulus);
  const std::size_t at = bytes_.size();
  bytes_.resize(at + size);
  for (std::uint32_t offset = 0; offset < size; ++offset) {
    bytes_[at + offset] = static_cast<std::byte>(core.mem_r8(address + offset));
  }
}

void ListJobWriter::save(Core &core) const {
  if (!named_) {
    return;
  }
  ListJobHeader header = header_;
  header.inputs = static_cast<std::uint32_t>(ranges_.size() / kRangeWords);
  psx::present::StateWriter state;
  state.put(header);
  state.putAll(std::span<const std::uint32_t>(ranges_));
  state.putAll(std::span<const std::byte>(bytes_));
  core.frameStates.save(core.emission.current(), state.bytes());
}

ListJob::ListJob(std::span<const std::byte> state) {
  psx::present::StateReader reader(state);
  header_ = reader.get<ListJobHeader>();
  const std::vector<std::uint32_t> ranges = reader.getAll<std::uint32_t>(std::size_t{header_.inputs} * kRangeWords);
  for (std::uint32_t input = 0; input < header_.inputs; ++input) {
    const std::uint32_t *range = ranges.data() + input * kRangeWords;
    inputs_.push_back(
        ListJobInput{range[0], static_cast<InputBlend>(range[2]), range[3], reader.getAll<std::byte>(range[1])});
  }
}

} // namespace tomba2::render
