// game/render/list_job.cpp — ListJobWriter and ListJob. See list_job.h.
#include "list_job.h"

#include "gte_state.h"
#include "guest_ordering_table.h"

#include <lucent/log.h>

#include <cstdlib>
#include <cstring>
#include <optional>

namespace tomba2::render {
namespace {

constexpr std::uint32_t kControlBase = 32;
constexpr std::uint32_t kRangeWords = 4; // address, size, blend, modulus

template <class Value> void append(std::vector<std::byte> &out, const Value &value) {
  const auto *bytes = reinterpret_cast<const std::byte *>(&value);
  out.insert(out.end(), bytes, bytes + sizeof(Value));
}

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
  GteRawState gte;
  GTE_SaveRawState(&gte);
  std::memcpy(header_.control, gte.reg + kControlBase, sizeof(header_.control));
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
  std::vector<std::byte> state;
  state.reserve(sizeof(header) + ranges_.size() * sizeof(std::uint32_t) + bytes_.size());
  append(state, header);
  for (const std::uint32_t word : ranges_) {
    append(state, word);
  }
  state.insert(state.end(), bytes_.begin(), bytes_.end());
  core.frameStates.save(core.emission.current(), std::span<const std::byte>(state));
}

ListJob::ListJob(std::span<const std::byte> state) {
  if (state.size() < sizeof(ListJobHeader)) {
    lucent::error("list-job", "a saved call of {} bytes is shorter than its header", state.size());
    std::abort();
  }
  std::memcpy(&header_, state.data(), sizeof(header_));
  std::size_t table = sizeof(header_);
  std::size_t payload = table + static_cast<std::size_t>(header_.inputs) * kRangeWords * sizeof(std::uint32_t);
  if (payload > state.size()) {
    lucent::error("list-job", "a saved call names {} inputs past its {} bytes", header_.inputs, state.size());
    std::abort();
  }
  for (std::uint32_t input = 0; input < header_.inputs; ++input, table += kRangeWords * sizeof(std::uint32_t)) {
    std::uint32_t range[kRangeWords];
    std::memcpy(range, state.data() + table, sizeof(range));
    if (payload + range[1] > state.size()) {
      lucent::error("list-job", "a saved call's input at 0x{:08X} runs past its bytes", range[0]);
      std::abort();
    }
    inputs_.push_back(
        ListJobInput{range[0], static_cast<InputBlend>(range[2]), range[3], state.subspan(payload, range[1])});
    payload += range[1];
  }
}

} // namespace tomba2::render
