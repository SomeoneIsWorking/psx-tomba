// game/render/object_draw.cpp — ObjectDrawScope and EmitterObject. See object_draw.h.
#include "object_draw.h"

#include "core.h"
#include "entry/session.h"

#include <cstdlib>
#include <lucent/log.h>

namespace tomba::render {

ObjectDrawScope::ObjectDrawScope(Core &core, std::uint32_t producer, std::uint32_t object)
    : core_(core), guard_(core.emission, producer, object, 0) {
  ObjectDraw &draw = sessionOf(&core).objectDraw;
  if (draw.open_) {
    lucent::error("object-draw", "object draw {:#010x} opened inside another", producer);
    std::abort();
  }
  draw.open_ = true;
}

ObjectDrawScope::~ObjectDrawScope() {
  sessionOf(&core_).objectDraw.open_ = false;
}

EmitterObject::EmitterObject(Core &core, std::uint32_t emitter, std::uint32_t list)
    : EmitterObject(core, emitter, OwnObject{core.emission.isOpen() ? core.emission.current().object : list}) {}

EmitterObject::EmitterObject(Core &core, std::uint32_t emitter, OwnObject object) {
  if (sessionOf(&core).objectDraw.isOpen()) {
    return;
  }
  guard_.emplace(core.emission, emitter, object.object, 0);
}

} // namespace tomba::render
