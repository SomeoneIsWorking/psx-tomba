// game/render/object_draw.h — the emission scope of one object drawn by several emitter calls.
// A billboard node's instances (FUN_8004CC88) are one draw whose billboard composes are its parts.
#pragma once

#include "emission_scope.h"

#include <cstdint>
#include <optional>

class Core;

namespace tomba::render {

class ObjectDraw {
public:
  bool isOpen() const {
    return open_;
  }

private:
  friend class ObjectDrawScope;
  bool open_ = false;
};

// Opens the object's scope and its draw; the emitter calls inside it draw under that scope.
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

} // namespace tomba::render
