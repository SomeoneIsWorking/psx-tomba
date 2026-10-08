#include "entry/tomba_identity.h"

namespace tomba::title {
namespace {

// Every value comes from config/tomba2-images.json through the CMake definitions.
constexpr psx::host::TitleIdentity kBootStub{
    .displayName = TOMBA2_TITLE_LABEL,
    .serial = TOMBA2_BOOT_NAME,
    .slug = "tomba2",
    .fileSize = TOMBA2_BOOT_SIZE,
    .sha256 = TOMBA2_BOOT_SHA256,
    .entry = TOMBA2_BOOT_ENTRY,
    .globalPointer = TOMBA2_BOOT_GP,
    .textAddress = TOMBA2_BOOT_TEXT_ADDRESS,
    .textSize = TOMBA2_BOOT_TEXT_SIZE,
    .stackAddress = TOMBA2_BOOT_STACK_ADDRESS,
    .stackOffset = TOMBA2_BOOT_STACK_OFFSET,
};

constexpr psx::host::TitleIdentity kMainExecutable{
    .displayName = TOMBA2_TITLE_LABEL,
    .serial = TOMBA2_MAIN_NAME,
    .slug = "tomba2",
    .fileSize = TOMBA2_MAIN_SIZE,
    .sha256 = TOMBA2_MAIN_SHA256,
    .entry = TOMBA2_MAIN_ENTRY,
    .globalPointer = TOMBA2_MAIN_GP,
    .textAddress = TOMBA2_MAIN_TEXT_ADDRESS,
    .textSize = TOMBA2_MAIN_TEXT_SIZE,
    .stackAddress = TOMBA2_MAIN_STACK_ADDRESS,
    .stackOffset = TOMBA2_MAIN_STACK_OFFSET,
};

} // namespace

const psx::host::TitleIdentity &bootStubIdentity() {
  return kBootStub;
}

const psx::host::TitleIdentity &mainExecutableIdentity() {
  return kMainExecutable;
}

} // namespace tomba::title
