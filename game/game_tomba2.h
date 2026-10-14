#pragma once

// Installs the Tomba! 2 native owners that are not part of an engine subsystem's own registration.
namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace tomba {

void games_tomba2_init(native::NativeOverrideCatalog &natives);

} // namespace tomba
