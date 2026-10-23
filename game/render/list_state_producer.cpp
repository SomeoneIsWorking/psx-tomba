// game/render/list_state_producer.cpp — ListStateProducer. See list_state_producer.h.
#include "list_state_producer.h"

#include "entry/session.h"
#include "gte_control.h"
#include "gte_registers.h"
#include "guest_ordering_table.h"
#include "packet_chains.h"

#include <array>
#include <cmath>
#include <cstring>
#include <optional>
#include <span>
#include <vector>

namespace tomba::render {
namespace {

constexpr std::uint32_t kScratchBase = 0x1F800000u;
constexpr std::uint32_t kScratchBytes = 0x400u;
constexpr std::uint32_t kOtBytes = OrderingTable::kBucketCount * 4u;
// The OT region also holds the tail the guest links its chains to; a body only writes bucket heads.
constexpr std::uint32_t kStackBelow = 0x100u;
constexpr std::uint32_t kStackAbove = 0x80u;
constexpr std::size_t kCameraWords = 8; // CR0-7

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

// Two s16 positions on a circle that repeats every `modulus`.
struct WrappedSpan {
  std::uint16_t from;
  std::uint16_t to;
  std::uint32_t modulus;
};
// The position `t` of the way from `from` to `to`, the short way round.
std::uint16_t lerpWrapped(const WrappedSpan &span, float t) {
  const auto [from, to, modulus] = span;
  const auto turn = static_cast<std::int32_t>(modulus);
  const std::int32_t gap = static_cast<std::int16_t>(to) - static_cast<std::int16_t>(from) + turn / 2;
  const std::int32_t delta = (gap % turn + turn) % turn - turn / 2;
  return static_cast<std::uint16_t>(static_cast<std::int16_t>(from) + psx::present::lerpInt(0, delta, t));
}

template <class Word> Word wordAt(std::span<const std::byte> bytes, std::size_t index) {
  Word word{};
  std::memcpy(&word, bytes.data() + index * sizeof(Word), sizeof(Word));
  return word;
}

template <class Word> void putWord(std::vector<std::byte> &bytes, std::size_t index, Word word) {
  std::memcpy(bytes.data() + index * sizeof(Word), &word, sizeof(Word));
}

// CR0-7 words `t` of the way between two saves: the rotation as the GTE control blend moves it.
void blendCamera(std::span<const std::byte> from, std::vector<std::byte> &to, float t) {
  psx::gte::RotationWords before{};
  psx::gte::RotationWords after{};
  for (std::size_t word = 0; word < psx::gte::kRotationWords; ++word) {
    before[word] = wordAt<std::uint32_t>(from, word);
    after[word] = wordAt<std::uint32_t>(to, word);
  }
  const psx::gte::RotationWords rotation = psx::present::rotationBetween(before, after, t);
  for (std::size_t word = 0; word < psx::gte::kRotationWords; ++word) {
    putWord(to, word, rotation[word]);
  }
  for (std::size_t word = psx::gte::kRotationWords; word < kCameraWords; ++word) {
    putWord(to, word, psx::present::lerpWord(wordAt<std::uint32_t>(from, word), wordAt<std::uint32_t>(to, word), t));
  }
}

// The bytes of input `index` of `to`, moved toward `from`'s by `t` as the input says.
std::vector<std::byte> blendedInput(std::size_t index, const ListJob &from, const ListJob &to, float t) {
  const ListJobInput &input = to.inputs()[index];
  const std::span<const std::byte> before = from.inputs()[index].bytes;
  std::vector<std::byte> bytes(input.bytes.begin(), input.bytes.end());
  if (t >= 1.0f) {
    return bytes;
  }
  switch (input.blend) {
  case InputBlend::Held:
    break;
  case InputBlend::Wrapped:
    if (bytes.size() == sizeof(std::uint16_t)) {
      const std::uint16_t moved = lerpWrapped(
          {.from = wordAt<std::uint16_t>(before, 0), .to = wordAt<std::uint16_t>(bytes, 0), .modulus = input.modulus},
          t);
      putWord(bytes, 0, moved);
    }
    break;
  case InputBlend::Lanes16:
    for (std::size_t lane = 0; lane < bytes.size() / sizeof(std::int16_t); ++lane) {
      putWord(bytes,
              lane,
              static_cast<std::int16_t>(
                  psx::present::lerpInt(wordAt<std::int16_t>(before, lane), wordAt<std::int16_t>(bytes, lane), t)));
    }
    break;
  case InputBlend::Camera:
    if (bytes.size() == kCameraWords * sizeof(std::uint32_t)) {
      blendCamera(before, bytes, t);
    }
    break;
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

void ListStateProducer::render(const psx::present::StateInterval &states, psx::present::PrimitiveSink &sink) const {
  const auto [from, to, t] = states;
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
                         ? blendedInput(index, *earlier, job, t)
                         : std::vector<std::byte>(job.inputs()[index].bytes.begin(), job.inputs()[index].bytes.end()));
  }

  const psx::present::Kept<psx::present::HostMemory>::Use host(sessionOf(&core_).renderMemory);
  host->zero(kScratchBase, kScratchBytes);
  for (std::size_t index = 0; index < inputs.size(); ++index) {
    const std::uint32_t address = job.inputs()[index].address;
    if (address - kScratchBase < kScratchBytes) {
      host->write(address, inputs[index].data(), static_cast<std::uint32_t>(inputs[index].size()));
    } else {
      host->provide(address, inputs[index]);
    }
  }
  std::array<std::byte, sizeof(std::uint32_t)> cursor{};
  std::memcpy(cursor.data(), &header.pool, sizeof(header.pool));
  host->provide(PacketPool::kCursor, cursor);
  host->zero(header.pool, header.arenaBytes);
  host->zero(header.call.ot, kOtBytes);
  host->zero(header.call.sp - kStackBelow, kStackBelow + kStackAbove);

  {
    const psx::present::GteGuard gte(core_.gte());
    const RegisterGuard registers(core_);
    psx::present::writeGteControl(core_.gte(), control);
    const psx::present::EmitMemory memory(core_, *host);
    emit(memory, header.variant, header.call);
    header.effect.apply(memory, header.pool, PacketPool(memory).cursor());
  }

  const psx::present::BucketHeads heads{header.call.ot,
                                        OrderingTable::kBucketCount,
                                        psx::present::OtSlot{static_cast<std::uint16_t>(header.table), header.slot}};
  psx::present::DrawModeTracker drawMode;
  psx::present::PacketChainWalker(psx::present::EmitMemory(core_, *host),
                                  psx::present::PacketBound::within({header.pool, header.arenaBytes}))
      .emitBuckets(heads, drawMode, sink);
}

} // namespace tomba::render
