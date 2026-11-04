// game/render/camera_lattice.cpp — CameraLattice. See camera_lattice.h.
#include "camera_lattice.h"

#include "core.h"
#include "gte_registers.h"

namespace tomba::render {
namespace {

constexpr std::uint32_t kCameraLead = 0x1F800104u;        // s16 x, y, z: the view's third row
constexpr std::uint32_t kCameraTranslation = 0x1F80010Cu; // s32 x, y, z
constexpr std::uint32_t kScratchTranslation = 0x1F800014u;
constexpr std::int32_t kHalfCube = 0x400;

std::int16_t axisCorner(const psx::present::EmitMemory &memory, std::uint32_t axis) {
  const std::int32_t lead = memory.mem_r16s(kCameraLead + axis * 2u) >> 1;
  return static_cast<std::int16_t>(memory.mem_r16(CameraLattice::kCameraPosition + axis * 4u) - kHalfCube + lead);
}

} // namespace

CameraLattice::Point CameraLattice::corner(const psx::present::EmitMemory &memory) {
  return {axisCorner(memory, 0u), axisCorner(memory, 1u), axisCorner(memory, 2u)};
}

CameraLattice::Offset
CameraLattice::nodeOffset(const psx::present::EmitMemory &memory, std::uint32_t node, Point corner) {
  return {memory.mem_r16s(node + kNodeOrigin) - corner.x,
          memory.mem_r16s(node + kNodeOrigin + 2u) - corner.y,
          memory.mem_r16s(node + kNodeOrigin + 4u) - corner.z};
}

void CameraLattice::loadView(const psx::present::EmitMemory &memory, Point corner) {
  psx::cpu::Gte &gte = memory.core().gte();
  for (std::uint32_t word = 0; word < psx::gte::kRotationWords; ++word) {
    gte.writeControl(psx::gte::kRotation + word, memory.mem_r32(kCameraMatrix + word * 4u));
  }
  gte.writeData(psx::gte::kVxy0,
                static_cast<std::uint16_t>(corner.x) |
                    (static_cast<std::uint32_t>(static_cast<std::uint16_t>(corner.y)) << 16));
  gte.writeData(psx::gte::kVz0, static_cast<std::uint16_t>(corner.z));
  gte.execute(psx::gte::kMvmvaRtV0);
  for (std::uint32_t axis = 0; axis < psx::gte::kTranslationWords; ++axis) {
    const std::uint32_t word = kScratchTranslation + axis * 4u;
    memory.mem_w32(word, gte.readData(psx::gte::kIr1 + axis) + memory.mem_r32(kCameraTranslation + axis * 4u));
    gte.writeControl(psx::gte::kTranslationX + axis, memory.mem_r32(word));
  }
}

} // namespace tomba::render
