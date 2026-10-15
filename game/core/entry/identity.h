#pragma once

#include "title_catalog.h"

namespace tomba::title {

// The disc's boot executable SCUS_944.54: what the catalog selects by.
const psx::host::TitleIdentity &bootStubIdentity();
// MAIN.EXE: the executable the stub loads, authenticated when the runtime loads it from the disc.
const psx::host::TitleIdentity &mainExecutableIdentity();

} // namespace tomba::title
