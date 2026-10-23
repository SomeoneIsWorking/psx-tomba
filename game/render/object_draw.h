// game/render/object_draw.h — one object drawn by several emitter calls, saved as one state.
// A08 FUN_80117628 writes one mesh at four turns, and a billboard composes four times a frame.
#pragma once

#include "emission_scope.h"
#include "state_producer.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

class Core;

namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba::render {

class ObjectDraw {
public:
  bool isOpen() const {
    return open_;
  }
  // Numbers the next call's part, for a draw whose calls draw its own numbered instances.
  void numberNext(std::uint32_t number) {
    next_ = number;
  }
  // The next emitter call of the open draw, rendered by `producer`.
  void beginPart(std::uint32_t producer);
  // The state of the current part.
  void addPart(std::span<const std::byte> state);

private:
  friend class ObjectDrawScope;
  bool open_ = false;
  std::uint32_t next_ = 0;     // number of the next call
  std::uint32_t number_ = 0;   // of the current call
  std::uint32_t producer_ = 0; // of the current call
  std::vector<std::byte> bytes_;
};

// Opens the object's scope and its draw; saves the parts under it when the draw ends.
class ObjectDrawScope {
public:
  ObjectDrawScope(Core &core, std::uint32_t producer, std::uint32_t object);
  ~ObjectDrawScope();
  ObjectDrawScope(const ObjectDrawScope &) = delete;
  ObjectDrawScope &operator=(const ObjectDrawScope &) = delete;
  ObjectDrawScope(ObjectDrawScope &&) = delete;
  ObjectDrawScope &operator=(ObjectDrawScope &&) = delete;

private:
  Core &core_;
  psx::present::EmissionScope::Guard guard_;
};

// An emitter call's own object, whatever scope encloses it: the node it draws.
struct OwnObject {
  std::uint32_t object;
};

// One emitter call's object: a part of the open draw, or (emitter, object) under its own scope.
class EmitterObject {
public:
  EmitterObject(Core &core, std::uint32_t emitter, std::uint32_t list);
  EmitterObject(Core &core, std::uint32_t emitter, OwnObject object);

private:
  std::optional<psx::present::EmissionScope::Guard> guard_;
};

// Saves the state of the emitter call now drawing: as the open draw's part, or under its own scope.
void saveEmitterState(Core &core, std::span<const std::byte> state);

// The render of an object draw's parts, each by its own producer from `producers`.
class ObjectDrawRender final : public psx::present::StateProducer {
public:
  explicit ObjectDrawRender(const psx::present::StateProducers &producers) : producers_(producers) {}

  void render(const psx::present::StateInterval &states, psx::present::PrimitiveSink &sink) const override;

private:
  const psx::present::StateProducers &producers_;
};

// The guest functions whose emitter calls are one object draw of the node in a0.
class ObjectDraws {
public:
  static void registerOverrides(native::NativeOverrideCatalog &natives);
  static void registerStateRenders(psx::present::StateProducerTable &renders);
};

} // namespace tomba::render
