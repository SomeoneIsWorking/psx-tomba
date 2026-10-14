// game/render/drop_shadow.cpp — DropShadow. See drop_shadow.h.
#include "drop_shadow.h"

#include "core.h"
#include "core/overrides/native_override_catalog.h"
#include "entry/tomba_session.h"
#include "gte_registers.h"
#include "guest_abi.h"
#include "obj_model_view.h"
#include "packed_mesh.h"

namespace tomba::render {
namespace {

constexpr GuestFrameSpill kSpills[] = {{18, 0x28}, {31, 0x30}, {19, 0x2C}, {17, 0x24}, {16, 0x20}};
constexpr std::uint32_t kFrameBytes = 0x38u;
constexpr std::uint32_t kReturnFromMesh = 0x8002ADF0u;

// Node fields.
constexpr std::uint32_t kExtentX = 0x40u;
constexpr std::uint32_t kExtentZ = 0x42u;
constexpr std::uint32_t kPosition = 0x48u;
constexpr std::uint32_t kFade = 0x4Eu;
constexpr std::uint32_t kSize = 0x50u;
constexpr std::uint32_t kAngles = 0x54u;

constexpr std::int32_t kOpaque = 0x80; // fade at which IR0 is 0
constexpr std::uint32_t kCueShift = 5u;
constexpr std::uint32_t kSizeShift = 11u;

void drawEntry(Core *core) {
  const psx::present::EmissionScope::Guard object(
      core->emission, DropShadow::kDraw, eng(core).graphicsBind.records.object(core->r[4]), 0);
  DropShadow::draw(*core);
}

} // namespace

void DropShadow::draw(Core &core) {
  const std::uint32_t node = core.r[4];
  GuestFrame<kFrameBytes, 5> frame(&core, kSpills);
  const std::int32_t fade = static_cast<std::int16_t>(core.mem_r16(node + kFade));
  core.mem_w32(PackedMesh::kDepthCue, static_cast<std::uint32_t>((kOpaque - fade) << kCueShift));
  core.gte().writeControl(psx::gte::kFarRed, 0u);
  core.gte().writeControl(psx::gte::kFarGreen, 0u);
  core.gte().writeControl(psx::gte::kFarBlue, 0u);
  const std::int32_t size = static_cast<std::int16_t>(core.mem_r16(node + kSize));
  const std::int32_t x = static_cast<std::int16_t>(core.mem_r16(node + kExtentX));
  const std::int32_t z = static_cast<std::int16_t>(core.mem_r16(node + kExtentZ));
  core.mem_w8(kScale, static_cast<std::uint8_t>((x * size) >> kSizeShift));
  core.mem_w8(kScale + 2u, static_cast<std::uint8_t>((z * size) >> kSizeShift));

  core.r[4] = node + kPosition;
  core.r[5] = kScale;
  core.r[6] = node + kAngles;
  ObjModelView::composeIntoGte(&core);

  core.r[4] = kMesh;
  core.r[5] = 0u;
  core.r[6] = 0u;
  core.r[7] = 0u;
  core.r[31] = kReturnFromMesh;
  PackedMesh::write(&core);
}

void DropShadow::registerOverrides(native::NativeOverrideCatalog &natives) {
  natives.declare(kDraw, "DropShadow::draw", &drawEntry);
}

} // namespace tomba::render
