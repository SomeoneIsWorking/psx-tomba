// game/render/object_draw.cpp — ObjectDraw and its render. See object_draw.h.
#include "object_draw.h"

#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "entry/session.h"
#include "native_dispatch.h"

#include <array>
#include <cstdlib>
#include <cstring>
#include <lucent/log.h>
#include <memory>

namespace tomba::render {
namespace {

// Each part: number, producer, size, then its state.
struct PartHeader {
  std::uint32_t number = 0;
  std::uint32_t producer = 0;
  std::uint32_t size = 0;
};

struct Part {
  PartHeader header;
  std::span<const std::byte> state;
};

constexpr std::size_t kMostParts = 32;

class Parts {
public:
  explicit Parts(std::span<const std::byte> bytes) {
    while (!bytes.empty()) {
      if (count_ == kMostParts || bytes.size() < sizeof(PartHeader)) {
        lucent::error("object-draw", "object draw state of {} parts is malformed", count_);
        std::abort();
      }
      Part &part = parts_[count_++];
      std::memcpy(&part.header, bytes.data(), sizeof(PartHeader));
      part.state = bytes.subspan(sizeof(PartHeader), part.header.size);
      bytes = bytes.subspan(sizeof(PartHeader) + part.header.size);
    }
  }
  std::span<const Part> all() const {
    return std::span<const Part>(parts_.data(), count_);
  }
  // The part of the same call by the same producer.
  const Part *partner(const PartHeader &of) const {
    for (const Part &part : all()) {
      if (part.header.number == of.number && part.header.producer == of.producer) {
        return &part;
      }
    }
    return nullptr;
  }

private:
  std::array<Part, kMostParts> parts_{};
  std::size_t count_ = 0;
};

// A08 FUN_80117628: one mesh at four quarter turns, again faded, then two packed meshes, for the node in a0.
constexpr std::uint32_t kQuarterTurnMesh = 0x80117628u;

void drawQuarterTurnMesh(Core *core) {
  const ObjectDrawScope draw(*core, kQuarterTurnMesh, eng(core).graphicsBind.records.object(core->r[4]));
  psx::cpu::callOriginalToReturn(
      *core, kQuarterTurnMesh, psx::cpu::ExecutionBudget::currentTurn(*core), "ObjectDraws::quarterTurnMesh");
}

} // namespace

void ObjectDraw::beginPart(std::uint32_t producer) {
  producer_ = producer;
  number_ = next_++;
}

void ObjectDraw::addPart(std::span<const std::byte> state) {
  const PartHeader header{number_, producer_, static_cast<std::uint32_t>(state.size())};
  const auto *raw = reinterpret_cast<const std::byte *>(&header);
  bytes_.insert(bytes_.end(), raw, raw + sizeof(header));
  bytes_.insert(bytes_.end(), state.begin(), state.end());
}

ObjectDrawScope::ObjectDrawScope(Core &core, std::uint32_t producer, std::uint32_t object)
    : core_(core), guard_(core.emission, producer, object, 0) {
  ObjectDraw &draw = sessionOf(&core).objectDraw;
  if (draw.open_) {
    lucent::error("object-draw", "object draw {:#010x} opened inside another", producer);
    std::abort();
  }
  draw.open_ = true;
  draw.next_ = 0;
  draw.bytes_.clear();
}

ObjectDrawScope::~ObjectDrawScope() {
  ObjectDraw &draw = sessionOf(&core_).objectDraw;
  if (!draw.bytes_.empty()) {
    core_.frameStates.save(core_.emission.current(), std::span<const std::byte>(draw.bytes_));
  }
  draw.open_ = false;
}

EmitterObject::EmitterObject(Core &core, std::uint32_t emitter, std::uint32_t list)
    : EmitterObject(core, emitter, OwnObject{core.emission.isOpen() ? core.emission.current().object : list}) {}

EmitterObject::EmitterObject(Core &core, std::uint32_t emitter, OwnObject object) {
  ObjectDraw &draw = sessionOf(&core).objectDraw;
  if (draw.isOpen()) {
    draw.beginPart(emitter);
    return;
  }
  guard_.emplace(core.emission, emitter, object.object, 0);
}

void saveEmitterState(Core &core, std::span<const std::byte> state) {
  ObjectDraw &draw = sessionOf(&core).objectDraw;
  if (draw.isOpen()) {
    draw.addPart(state);
    return;
  }
  core.frameStates.save(core.emission.current(), state);
}

void ObjectDrawRender::render(const psx::present::StateInterval &states, psx::present::PrimitiveSink &sink) const {
  const auto [from, to, t] = states;
  const Parts later(to);
  const Parts earlier(from);
  // A bucket walks its last-linked packet first, so the last call's primitives lead.
  const std::span<const Part> parts = later.all();
  for (auto part = parts.rbegin(); part != parts.rend(); ++part) {
    const psx::present::StateProducer *producer = producers_.find(part->header.producer);
    if (producer == nullptr) {
      lucent::error("object-draw", "object draw part of {:#010x} has no render", part->header.producer);
      std::abort();
    }
    const Part *partner = earlier.partner(part->header);
    producer->render({.from = partner != nullptr ? partner->state : part->state, .to = part->state, .t = t}, sink);
  }
}

} // namespace tomba::render

namespace tomba::render {

void ObjectDraws::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declareOverlay("A08", kQuarterTurnMesh, "ObjectDraws::quarterTurnMesh", &drawQuarterTurnMesh);
}

void ObjectDraws::registerStateRenders(psx::present::StateProducerTable &renders) {
  renders.install(kQuarterTurnMesh, std::make_unique<ObjectDrawRender>(renders));
}

} // namespace tomba::render
