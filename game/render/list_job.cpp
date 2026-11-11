// game/render/list_job.cpp — ListCall. See list_job.h.
#include "list_job.h"

namespace tomba::render {

ListCall ListCall::fromRegisters(const Core &core) {
  return ListCall{core.r[4], core.r[5], core.r[6], core.r[7], core.r[29], core.r[5]};
}

} // namespace tomba::render
