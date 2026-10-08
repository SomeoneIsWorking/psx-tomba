// Tomba2Engine — widescreen-margin re-include set.
//
// The game culls objects outside the 4:3 frustum, so at 16:9 the widened side geometry is dropped.
// Poking a culled object's +1 render flag draws it but runs the handler's visible branch and changes
// gameplay state (docs/journal.md later-133). So the cull COLLECTS the re-include-eligible nodes here
// instead; the widescreen margin producer will draw them.
//
// One instance per Core, owned by Render (`rend(c)->margin`).
#ifndef PSXPORT_MARGIN_RENDER_HPP
#define PSXPORT_MARGIN_RENDER_HPP
#include "core.h"
#include <stdint.h>
#include <unordered_set>
#include <vector>

class MarginRenderer {
public:
  // True (default) to collect instead of poking +1; PSXPORT_MARGIN_POKE=1 selects the poke.
  int nativeEnabled();

  // Called from the cull for an object the wide frustum re-includes. Records the node for the
  // post-walk flush. Deduped per frame (the cull runs several times per object via the submit
  // wrappers). Takes the Core to read the node's type from this instance's RAM.
  void collect(Core *c, uint32_t node);

  // Called from ObjectList::walkAll after both list walks: ends the frame's collection.
  void flush(Core *c);

  bool dbg_ = false;

private:
  int mNativeEnabled = -1; // lazy PSXPORT_MARGIN_POKE latch (-1 = not read yet)
  std::vector<uint32_t> nodes_;
  std::unordered_set<uint32_t> seen_;
};
#endif
