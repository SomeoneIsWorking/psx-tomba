// game/render/object_draw.cpp — ObjectDraw and its render. See object_draw.h.
#include "object_draw.h"

#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "entry/session.h"
#include "native_dispatch.h"

#include <cstdlib>
#include <cstring>
#include <lucent/log.h>
#include <memory>
#include <optional>
#include <vector>

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

Part partAt(std::span<const std::byte> bytes, std::size_t offset) {
  Part part;
  std::memcpy(&part.header, bytes.data() + offset, sizeof(PartHeader));
  part.state = bytes.subspan(offset + sizeof(PartHeader), part.header.size);
  return part;
}

// Each part's offset in `bytes`; a strip draws one part per segment, so the count has no fixed bound.
void readParts(std::span<const std::byte> bytes, std::vector<std::size_t> &offsets) {
  offsets.clear();
  std::size_t offset = 0;
  while (offset < bytes.size()) {
    PartHeader header;
    if (bytes.size() - offset < sizeof(PartHeader)) {
      lucent::error("object-draw", "object draw state of {} parts is malformed", offsets.size());
      std::abort();
    }
    std::memcpy(&header, bytes.data() + offset, sizeof(PartHeader));
    if (bytes.size() - offset - sizeof(PartHeader) < header.size) {
      lucent::error("object-draw", "object draw state of {} parts is malformed", offsets.size());
      std::abort();
    }
    offsets.push_back(offset);
    offset += sizeof(PartHeader) + header.size;
  }
}

// The part of the same call by the same producer; parts are numbered in call order, so its index is the guess.
std::optional<Part>
partner(std::span<const std::byte> bytes, std::span<const std::size_t> offsets, const PartHeader &of) {
  const auto matches = [&](std::size_t offset) {
    const Part part = partAt(bytes, offset);
    return part.header.number == of.number && part.header.producer == of.producer;
  };
  if (of.number < offsets.size() && matches(offsets[of.number])) {
    return partAt(bytes, offsets[of.number]);
  }
  for (const std::size_t offset : offsets) {
    if (matches(offset)) {
      return partAt(bytes, offset);
    }
  }
  return std::nullopt;
}

// A08 FUN_80117628: one mesh at four quarter turns, again faded, then two packed meshes, for the node in a0.
constexpr std::uint32_t kQuarterTurnMesh = 0x80117628u;

// The hanging strip: one world quad per segment, swaying with the node. A0F's body is A0A's sway branch.
constexpr std::uint32_t kHangingStripA0a = 0x80114E64u;
constexpr std::uint32_t kHangingStripA0f = 0x801175C0u;

void drawParts(Core &core, std::uint32_t drawer, const char *owner) {
  const ObjectDrawScope draw(core, drawer, eng(&core).graphicsBind.records.object(core.r[4]));
  psx::cpu::callOriginalToReturn(core, drawer, psx::cpu::ExecutionBudget::currentTurn(core), owner);
}

void drawHangingStripA0a(Core *core) {
  drawParts(*core, kHangingStripA0a, "ObjectDraws::hangingStrip");
}

void drawHangingStripA0f(Core *core) {
  drawParts(*core, kHangingStripA0f, "ObjectDraws::hangingStrip");
}

void drawQuarterTurnMesh(Core *core) {
  drawParts(*core, kQuarterTurnMesh, "ObjectDraws::quarterTurnMesh");
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
  readParts(to, laterParts_);
  readParts(from, earlierParts_);
  // A bucket walks its last-linked packet first, so the last call's primitives lead.
  for (auto offset = laterParts_.rbegin(); offset != laterParts_.rend(); ++offset) {
    const Part part = partAt(to, *offset);
    const psx::present::StateProducer *producer = producers_.find(part.header.producer);
    if (producer == nullptr) {
      lucent::error("object-draw", "object draw part of {:#010x} has no render", part.header.producer);
      std::abort();
    }
    const std::optional<Part> earlier = partner(from, earlierParts_, part.header);
    producer->render({.from = earlier ? earlier->state : part.state, .to = part.state, .t = t}, sink);
  }
}

} // namespace tomba::render

namespace tomba::render {

void ObjectDraws::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declareOverlay("A08", kQuarterTurnMesh, "ObjectDraws::quarterTurnMesh", &drawQuarterTurnMesh);
  natives.declareOverlay("A0A", kHangingStripA0a, "ObjectDraws::hangingStrip", &drawHangingStripA0a);
  natives.declareOverlay("A0F", kHangingStripA0f, "ObjectDraws::hangingStrip", &drawHangingStripA0f);
}

void ObjectDraws::registerStateRenders(psx::present::StateProducerTable &renders) {
  renders.install(kQuarterTurnMesh, std::make_unique<ObjectDrawRender>(renders));
  renders.install(kHangingStripA0a, std::make_unique<ObjectDrawRender>(renders));
  renders.install(kHangingStripA0f, std::make_unique<ObjectDrawRender>(renders));
}

} // namespace tomba::render
