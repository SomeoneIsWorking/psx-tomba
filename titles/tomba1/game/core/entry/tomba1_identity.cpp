#include "tomba1_identity.h"

namespace tomba1 {
namespace {

// Every value comes from titles/tomba1/executable.json through the CMake definitions.
constexpr psx::host::TitleIdentity kIdentity{
    .displayName = TOMBA1_TITLE,
    .serial = TOMBA1_EXECUTABLE_NAME,
    .slug = "tomba1",
    .fileSize = TOMBA1_FILE_SIZE,
    .sha256 = TOMBA1_SHA256,
    .entry = TOMBA1_ENTRY,
    .globalPointer = TOMBA1_GLOBAL_POINTER,
    .textAddress = TOMBA1_TEXT_ADDRESS,
    .textSize = TOMBA1_TEXT_SIZE,
    .stackAddress = TOMBA1_STACK_ADDRESS,
    .stackOffset = TOMBA1_STACK_SIZE,
    .discEnvVar = "PSXPORT_TOMBA1_DISC",
    .discExecutablePath = TOMBA1_EXECUTABLE_NAME,
};

} // namespace

const psx::host::TitleIdentity &identity() {
  return kIdentity;
}

} // namespace tomba1
