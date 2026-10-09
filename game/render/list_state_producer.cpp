// game/render/list_state_producer.cpp — ListStateProducer. See list_state_producer.h.
#include "list_state_producer.h"

#include "gp0_primitive_decode.h"
#include "gte_state.h"
#include "guest_ordering_table.h"

#include <lucent/log.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
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
constexpr std::uint32_t kRotationWords = 5u;
constexpr std::uint32_t kTranslationEnd = 8u;
constexpr std::uint32_t kFlagRegister = 31u;
constexpr std::uint32_t kTagWordShift = 24u;
constexpr std::uint32_t kTagNextMask = 0xFFFFFFu;
constexpr std::uint32_t kDrawModeCommand = 0xE1u;

// The GTE as the render found it, handed back when the render is done.
class GteGuard {
public:
  GteGuard() {
    GTE_SaveRawState(&saved_);
  }
  ~GteGuard() {
    GTE_RestoreRawState(&saved_);
  }
  GteGuard(const GteGuard &) = delete;
  GteGuard &operator=(const GteGuard &) = delete;

private:
  GteRawState saved_{};
};

std::int32_t lerpInt(std::int32_t from, std::int32_t to, float t) {
  return from + static_cast<std::int32_t>(std::lround(static_cast<double>(to - from) * static_cast<double>(t)));
}

// Rotation words hold two s16 elements each; translation words are s32.
std::uint32_t lerpControl(std::uint32_t reg, std::uint32_t from, std::uint32_t to, float t) {
  if (reg >= kTranslationEnd) {
    return to;
  }
  if (reg >= kRotationWords) {
    return static_cast<std::uint32_t>(lerpInt(static_cast<std::int32_t>(from), static_cast<std::int32_t>(to), t));
  }
  std::uint32_t word = 0;
  for (unsigned half = 0; half < 2; ++half) {
    const auto a = static_cast<std::int16_t>(from >> (half * 16u));
    const auto b = static_cast<std::int16_t>(to >> (half * 16u));
    word |= static_cast<std::uint32_t>(static_cast<std::uint16_t>(lerpInt(a, b, t))) << (half * 16u);
  }
  return word;
}

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
  return static_cast<std::uint16_t>(static_cast<std::int16_t>(from) + lerpInt(0, delta, t));
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
  std::array<std::uint32_t, 32> control{};
  std::copy(std::begin(header.control), std::end(header.control), control.begin());
  std::vector<std::vector<std::byte>> inputs;
  const std::optional<ListJob> earlier = from.data() != to.data() ? std::optional<ListJob>(from) : std::nullopt;
  const bool paired = earlier && samePart(*earlier, job);
  if (paired) {
    for (std::uint32_t reg = 0; reg < kTranslationEnd; ++reg) {
      control[reg] = lerpControl(reg, earlier->header().control[reg], header.control[reg], t);
    }
  }
  inputs.reserve(job.inputs().size());
  for (std::size_t index = 0; index < job.inputs().size(); ++index) {
    inputs.push_back(paired
                         ? blendedInput(*earlier, job, index, t)
                         : std::vector<std::byte>(job.inputs()[index].bytes.begin(), job.inputs()[index].bytes.end()));
  }

  HostMemory host;
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
    const GteGuard gte;
    const RegisterGuard registers(core_);
    for (std::uint32_t reg = 0; reg < control.size(); ++reg) {
      if (reg != kFlagRegister) {
        gte_write_ctrl(reg, control[reg]);
      }
    }
    emit(EmitMemory(core_, host), header.variant, header.call);
  }

  // A sprite or line takes the texture page of the last draw mode packet before it in the chain.
  std::uint16_t drawMode = 0;
  const std::span<const std::byte> heads = host.view(header.call.ot, kOtBytes);
  for (std::uint32_t bucket = OrderingTable::kBucketCount; bucket-- > 0;) {
    std::uint32_t packet = 0;
    std::memcpy(&packet, heads.data() + bucket * 4u, sizeof(packet));
    for (std::uint32_t guard = 0; packet != 0; ++guard) {
      std::uint32_t tag = 0;
      host.read(packet, &tag, sizeof(tag));
      const std::uint32_t words = tag >> kTagWordShift;
      std::array<std::uint32_t, 16> command{};
      if (words > command.size() || guard > header.arenaBytes) {
        lucent::error("list-state", "packet at 0x{:08X} in bucket {} has {} words", packet, bucket, words);
        std::abort();
      }
      host.read(packet + 4u, command.data(), words * 4u);
      if (words > 0 && command[0] >> kTagWordShift == kDrawModeCommand) {
        drawMode = static_cast<std::uint16_t>(command[0]);
        packet = tag & kTagNextMask;
        continue;
      }
      auto primitive = psx::gpu::decodePacketPrimitive(std::span<const std::uint32_t>(command.data(), words));
      if (!primitive) {
        lucent::error("list-state", "packet at 0x{:08X} in bucket {} is not a polygon or sprite", packet, bucket);
        std::abort();
      }
      if (primitive->kind != psx::present::PrimitiveKind::Polygon) {
        psx::gpu::applyTexPageAttribute(primitive->state, drawMode);
      }
      sink.emit(psx::present::OtSlot{static_cast<std::uint16_t>(header.table), header.slot + bucket}, *primitive);
      packet = tag & kTagNextMask;
    }
  }
}

} // namespace tomba2::render
