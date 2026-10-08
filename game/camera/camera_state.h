// game/camera/camera_state.h — the guest-memory view the camera cluster works through.
//
// Every camera owner reads and writes the same two things: guest memory, and one camera object's
// fields at a scene-dependent base. This value type is that view, so the owners share one spelling
// of "read a guest word" and one spelling of "read a camera field" instead of repeating twelve
// accessors each.
#pragma once

#include "core.h"

#include <cstdint>

namespace tomba::camera {

// The GLOBAL camera/scene state block: master position (the player and the camera centre share it
// in this 2.5D game), scene heading, and the mode selectors.
inline constexpr uint32_t kCamGlobal = 0x800E7E80u;
// The SCRATCHPAD camera scratch block: the follow accumulators and the composed view matrix. The
// projection and the cull read it, so writes here are a content interface.
inline constexpr uint32_t kCamScratch = 0x1F8000D0u;

// Master world position, 16.16 fixed, in kCamGlobal.
inline constexpr uint32_t kMasterX = kCamGlobal + 0x2C; // 0x800E7EAC
inline constexpr uint32_t kMasterY = kCamGlobal + 0x30; // 0x800E7EB0
inline constexpr uint32_t kMasterZ = kCamGlobal + 0x34; // 0x800E7EB4

// The camera object the resident per-frame driver (0x8006EC44) hardcodes. Other callers pass their
// own base; the object layout is identical, only the driver hardcodes this one.
inline constexpr uint32_t kCameraObject = 0x800E8008u;

class CameraState {
public:
  CameraState() = default;
  CameraState(Core &core, uint32_t cameraObject) : c(&core), cam_(cameraObject) {}

protected:
  // The CPU the camera runs on, and the camera object this owner operates. Both are borrowed; the
  // camera object outlives every owner because it is a guest-RAM struct.
  Core *c = nullptr;
  uint32_t cam_ = 0;

public:
  uint8_t r8(uint32_t a) const {
    return c->mem_r8(a);
  }
  uint16_t r16(uint32_t a) const {
    return c->mem_r16(a);
  }
  uint32_t r32(uint32_t a) const {
    return c->mem_r32(a);
  }
  void w8(uint32_t a, uint8_t v) {
    c->mem_w8(a, v);
  }
  void w16(uint32_t a, uint16_t v) {
    c->mem_w16(a, v);
  }
  void w32(uint32_t a, uint32_t v) {
    c->mem_w32(a, v);
  }

  uint8_t camR8(uint32_t off) const {
    return c->mem_r8(cam_ + off);
  }
  uint16_t camR16(uint32_t off) const {
    return c->mem_r16(cam_ + off);
  }
  uint32_t camR32(uint32_t off) const {
    return c->mem_r32(cam_ + off);
  }
  void camW8(uint32_t off, uint8_t v) {
    c->mem_w8(cam_ + off, v);
  }
  void camW16(uint32_t off, uint16_t v) {
    c->mem_w16(cam_ + off, v);
  }
  void camW32(uint32_t off, uint32_t v) {
    c->mem_w32(cam_ + off, v);
  }
};

} // namespace tomba::camera