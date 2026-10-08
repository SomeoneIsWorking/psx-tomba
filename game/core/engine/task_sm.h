#pragma once
#include "core.h"
#include <cstdint>

// TaskSm — typed lens over the GAME task-state-machine record at *0x1F800138
// (guest addresses documented at the top of this file: sm[0x48] top state,
// sm[0x4a] running sub-mode, sm[0x4c] area machine, sm[0x4e]/[0x50]
// area-machine sub-state, sm[0x5c] intro timer, sm[0x69]/[0x6b] flag bytes). A
// pure re-read wrapper — same guest addresses as the raw c->mem_r/w calls it
// replaces, not a cache; construct one per use (`TaskSm sm(c);`) exactly like
// the raw `mem_r32(0x1f800138)` it stands in for.
class TaskSm {
public:
  // The pointer to the task record, read at CONSTRUCTION. Every accessor then works from that one
  // base, which is why a body that re-derives the lens mid-function is re-reading the pointer and not
  // reusing a cached record.
  static constexpr uint32_t kTaskRecordPointer = 0x1F800138u;

  explicit TaskSm(Core *c_) : c(c_), base(c_->mem_r32(kTaskRecordPointer)) {}

  Core *c;
  uint32_t base;

  uint16_t top() const {
    return c->mem_r16(base + 0x48u);
  }
  void setTop(uint16_t v) {
    c->mem_w16(base + 0x48u, v);
  }

  uint16_t subMode() const {
    return c->mem_r16(base + 0x4Au);
  }
  void setSubMode(uint16_t v) {
    c->mem_w16(base + 0x4Au, v);
  }

  uint16_t stage4c() const {
    return c->mem_r16(base + 0x4Cu);
  }
  void setStage4c(uint16_t v) {
    c->mem_w16(base + 0x4Cu, v);
  }

  uint16_t s4e() const {
    return c->mem_r16(base + 0x4Eu);
  }
  void setS4e(uint16_t v) {
    c->mem_w16(base + 0x4Eu, v);
  }

  uint16_t s50() const {
    return c->mem_r16(base + 0x50u);
  }
  void setS50(uint16_t v) {
    c->mem_w16(base + 0x50u, v);
  }

  uint16_t introTimer() const {
    return c->mem_r16(base + 0x5Cu);
  }
  void setIntroTimer(uint16_t v) {
    c->mem_w16(base + 0x5Cu, v);
  }

  uint8_t f69() const {
    return c->mem_r8(base + 0x69u);
  }
  void setF69(uint8_t v) {
    c->mem_w8(base + 0x69u, v);
  }

  uint8_t f6b() const {
    return c->mem_r8(base + 0x6Bu);
  }
  void setF6b(uint8_t v) {
    c->mem_w8(base + 0x6Bu, v);
  }
};
