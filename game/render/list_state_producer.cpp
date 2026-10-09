// game/render/list_state_producer.cpp — ListStateProducer. See list_state_producer.h.
#include "list_state_producer.h"

#include "gte_control.h"
#include "guest_ordering_table.h"
#include "host_ordering_table.h"

#include <array>
#include <cmath>
#include <cstring>
#include <optional>
#include <vector>

namespace tomba2::render {
namespace {

constexpr std::uint32_t kScratchBase = 0x1F800000u;
constexpr std::uint32_t kScratchBytes = 0x400u;
constexpr std::uint32_t kOtBytes = OrderingTable::kBucketCount * 4u;
// The OT region also holds the tail the guest links its chains to; a body only writes bucket heads.
constexpr std::uint32_t kStackBelow = 0x100u;
constexpr std::uint32_t kStackAbove = 0x80u;

// Both calls drew the same list through the same body.
bool samePart(const ListJob &a, const ListJob &b) {
  const ListJobHeader &left = a.header();
  const ListJobHeader &right = b.header();
  if (left.variant != right.variant || left.call.list != right.call.list || left.call.count != right.call.count ||
      a.inputs().size() != b.inputs().size()) {
    return false;
  }
  for (std::size_t input = 0; input < a.inputs().size(); ++input) {
    if (a.inputs()[input].address != b.inputs()[input].address ||
        a.inputs()[input].bytes.size() != b.inputs()[input].bytes.size() ||
        a.inputs()[input].modulus != b.inputs()[input].modulus) {
      return false;
    }
  }
  return true;
}

// An s16 position `t` of the way from `from` to `to`, the short way round a repeat of `modulus`.
std::uint16_t lerpWrapped(std::uint16_t from, std::uint16_t to, std::uint32_t modulus, float t) {
  const auto turn = static_cast<std::int32_t>(modulus);
  const std::int32_t gap = static_cast<std::int16_t>(to) - static_cast<std::int16_t>(from) + turn / 2;
  const std::int32_t delta = (gap % turn + turn) % turn - turn / 2;
  return static_cast<std::uint16_t>(static_cast<std::int16_t>(from) + psx::present::lerpInt(0, delta, t));
}

// The bytes of input `index` of `to`, moved toward `from`'s by `t` as the input says.
std::vector<std::byte> blendedInput(const ListJob &from, const ListJob &to, std::size_t index, float t) {
  const ListJobInput &input = to.inputs()[index];
  std::vector<std::byte> bytes(input.bytes.begin(), input.bytes.end());
  if (input.blend == InputBlend::Wrapped && bytes.size() == sizeof(std::uint16_t) && t < 1.0f) {
    std::uint16_t before = 0;
    std::uint16_t after = 0;
    std::memcpy(&before, from.inputs()[index].bytes.data(), sizeof(before));
    std::memcpy(&after, bytes.data(), sizeof(after));
    const std::uint16_t moved = lerpWrapped(before, after, input.modulus, t);
    std::memcpy(bytes.data(), &moved, sizeof(moved));
  }
  return bytes;
}

// The guest registers a render's body may write; handed back when the render is done.
class RegisterGuard {
public:
  explicit RegisterGuard(Core &core) : core_(core), lo_(core.lo), hi_(core.hi) {}
  ~RegisterGuard() {
    core_.lo = lo_;
    core_.hi = hi_;
  }
  RegisterGuard(const RegisterGuard &) = delete;
  RegisterGuard &operator=(const RegisterGuard &) = delete;

private:
  Core &core_;
  std::uint32_t lo_;
  std::uint32_t hi_;
};

} // namespace

void ListStateProducer::render(std::span<const std::byte> from,
                               std::span<const std::byte> to,
                               float t,
                               psx::present::PrimitiveSink &sink) const {
  const ListJob job(to);
  const ListJobHeader &header = job.header();
  psx::present::GteControl control = header.control;
  std::vector<std::vector<std::byte>> inputs;
  const std::optional<ListJob> earlier = from.data() != to.data() ? std::optional<ListJob>(from) : std::nullopt;
  const bool paired = earlier && samePart(*earlier, job);
  if (paired) {
    control = psx::present::blendGteControl(earlier->header().control, header.control, t);
  }
  inputs.reserve(job.inputs().size());
  for (std::size_t index = 0; index < job.inputs().size(); ++index) {
    inputs.push_back(paired
                         ? blendedInput(*earlier, job, index, t)
                         : std::vector<std::byte>(job.inputs()[index].bytes.begin(), job.inputs()[index].bytes.end()));
  }

  psx::present::HostMemory host;
  host.zero(kScratchBase, kScratchBytes);
  for (std::size_t index = 0; index < inputs.size(); ++index) {
    const std::uint32_t address = job.inputs()[index].address;
    if (address - kScratchBase < kScratchBytes) {
      host.write(address, inputs[index].data(), static_cast<std::uint32_t>(inputs[index].size()));
    } else {
      host.provide(address, inputs[index]);
    }
  }
  std::array<std::byte, sizeof(std::uint32_t)> cursor{};
  std::memcpy(cursor.data(), &header.pool, sizeof(header.pool));
  host.provide(PacketPool::kCursor, cursor);
  host.zero(header.pool, header.arenaBytes);
  host.zero(header.call.ot, kOtBytes);
  host.zero(header.call.sp - kStackBelow, kStackBelow + kStackAbove);

  {
    const psx::present::GteGuard gte;
    const RegisterGuard registers(core_);
    psx::present::writeGteControl(control);
    emit(psx::present::EmitMemory(core_, host), header.variant, header.call);
  }

  const psx::present::HostOrderingTable table{
      header.call.ot,
      OrderingTable::kBucketCount,
      psx::present::OtSlot{static_cast<std::uint16_t>(header.table), header.slot},
      header.arenaBytes};
  psx::present::emitHostOrderingTable(host, table, sink);
}

} // namespace tomba2::render
