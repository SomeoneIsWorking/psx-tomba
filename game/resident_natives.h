#pragma once

// Installs the Tomba! 2 native owners that are not part of an engine subsystem's own registration.
namespace tomba::native {
class NativeOverrideCatalog;
} // namespace tomba::native

namespace psx::present {
class StateProducerTable;
} // namespace psx::present

namespace tomba {

void installResidentNatives(native::NativeOverrideCatalog &natives);
// The text-label render fn's state producer (FUN_80039F4C).
void text_label_register_state_renders(psx::present::StateProducerTable &renders);

} // namespace tomba
