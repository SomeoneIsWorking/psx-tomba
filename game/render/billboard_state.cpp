// game/render/billboard_state.cpp — BillboardState. See billboard_state.h.
#include "billboard_state.h"

#include "blend.h"
#include "gp0_primitive_decode.h"
#include "gte_control.h"
#include "gte_registers.h"
#include "guest_ordering_table.h"
#include "state_bytes.h"

#include <algorithm>
#include <cstdlib>
#include <lucent/log.h>
#include <memory>

namespace tomba::render {
namespace {

constexpr std::uint32_t kParticleWords = BillboardState::kCornerWords + 1u + BillboardState::kPacketWords;
constexpr std::uint32_t kBucketWord = BillboardState::kCornerWords;
constexpr std::uint32_t kCommandWord = BillboardState::kCornerWords + 1u;
// The packet words holding the four projected corners.
constexpr std::uint32_t kCornerXy[4] = {1u, 3u, 5u, 7u};

struct Header {
  std::uint32_t table = 0;
  std::uint32_t slot = 0; // OT slot of bucket 0
  std::uint32_t particles = 0;
  psx::present::GteControl control{};
};

struct Saved {
  Header header;
  std::vector<std::uint32_t> particles;

  explicit Saved(std::span<const std::byte> state) {
    psx::present::StateReader reader(state);
    header = reader.get<Header>();
    particles = reader.getAll<std::uint32_t>(std::size_t{header.particles} * kParticleWords);
  }
};

class BillboardRender final : public psx::present::StateProducer {
public:
  explicit BillboardRender(Core &core) : core_(core) {}

  void render(const psx::present::StateInterval &states, psx::present::PrimitiveSink &sink) const override {
    const auto [from, to, t] = states;
    const Saved later(to);
    std::vector<std::uint32_t> corners(later.particles);
    psx::present::GteControl control = later.header.control;
    if (from.data() != to.data()) {
      const Saved earlier(from);
      if (earlier.header.particles == later.header.particles) {
        control = psx::present::blendGteControl(earlier.header.control, later.header.control, t);
        for (std::size_t particle = 0; particle < later.header.particles; ++particle) {
          for (std::uint32_t word = 0; word < BillboardState::kCornerWords; ++word) {
            const std::size_t at = particle * kParticleWords + word;
            corners[at] = psx::present::lerpHalves(earlier.particles[at], later.particles[at], t);
          }
        }
      }
    }

    const psx::present::GteGuard gte;
    psx::present::writeGteControl(control);
    // The last particle linked heads its bucket, so the walk meets the particles last first.
    for (std::size_t particle = later.header.particles; particle-- > 0;) {
      const std::uint32_t *saved = later.particles.data() + particle * kParticleWords;
      const std::uint32_t *moved = corners.data() + particle * kParticleWords;
      std::uint32_t command[BillboardState::kPacketWords];
      std::copy_n(saved + kCommandWord, BillboardState::kPacketWords, command);
      for (std::uint32_t reg = 0; reg < 6u; ++reg) {
        core_.gte().writeData(reg, moved[reg]);
      }
      core_.gte().execute(psx::gte::kRtpt);
      command[kCornerXy[0]] = core_.gte().readData(psx::gte::kSxy0);
      command[kCornerXy[1]] = core_.gte().readData(psx::gte::kSxy1);
      command[kCornerXy[2]] = core_.gte().readData(psx::gte::kSxy2);
      core_.gte().writeData(psx::gte::kVxy0, moved[6]);
      core_.gte().writeData(psx::gte::kVz0, moved[7]);
      core_.gte().execute(psx::gte::kRtps);
      command[kCornerXy[3]] = core_.gte().readData(psx::gte::kSxy2);
      auto quad = psx::gpu::decodePacketPrimitive(command);
      if (!quad) {
        lucent::error("billboard", "saved billboard packet 0x{:08X} did not decode", command[0]);
        std::abort();
      }
      sink.emit(
          psx::present::OtSlot{static_cast<std::uint16_t>(later.header.table), later.header.slot + saved[kBucketWord]},
          *quad);
    }
  }

private:
  Core &core_;
};

} // namespace

BillboardState::BillboardState() : control_(psx::present::readGteControl()) {}

void BillboardState::add(std::span<const std::uint32_t, kCornerWords> corners,
                         std::uint32_t bucket,
                         std::span<const std::uint32_t, kPacketWords> command) {
  particles_.insert(particles_.end(), corners.begin(), corners.end());
  particles_.push_back(bucket);
  particles_.insert(particles_.end(), command.begin(), command.end());
}

void BillboardState::save(Core &core) const {
  const auto slot = core.otTables.slotOf(OrderingTable::active(core).base());
  if (!slot) {
    return;
  }
  psx::present::StateWriter state;
  state.put(Header{slot->table, slot->index, static_cast<std::uint32_t>(particles_.size() / kParticleWords), control_});
  state.putAll(std::span<const std::uint32_t>(particles_));
  core.frameStates.save(core.emission.current(), state.bytes());
}

void BillboardState::registerStateRenders(psx::present::StateProducerTable &renders, Core &core) {
  renders.install(kEmitter, std::make_unique<BillboardRender>(core));
}

} // namespace tomba::render
