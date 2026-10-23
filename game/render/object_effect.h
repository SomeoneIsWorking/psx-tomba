// game/render/object_effect.h — the effect a node's dispatcher applies over the packets its emission wrote.
// FUN_8003CCA4 / FUN_8003EF9C hand the packet span a node emitted to one of five leaves.
#pragma once

#include "emit_memory.h"

#include <cstdint>

class Core;

namespace tomba::render {

class ObjectEffect {
public:
  ObjectEffect() = default;

  // FUN_8003D584: each colour channel offset by the node's amount at +0x18..+0x1A.
  static ObjectEffect colorAdd(Core &core, std::uint32_t node);
  // FUN_8003F344: the node's CLUT at +0x5C.
  static ObjectEffect clutSwap(Core &core, std::uint32_t node);
  // FUN_8003F3F4 (on) / FUN_8003F4C4 (off): the semi-transparency bit.
  static ObjectEffect semi(bool on);
  // FUN_8003F594: every colour word set to the node's word at +0x18, semi-transparency on.
  static ObjectEffect flatTint(Core &core, std::uint32_t node);

  bool operator==(const ObjectEffect &) const = default;

  // Rewrites every colour-bearing packet in [lo, hi); over the guest each packet keeps its key.
  void apply(const psx::present::EmitMemory &memory, std::uint32_t lo, std::uint32_t hi) const;

private:
  enum class Kind : std::uint8_t { None, ColorAdd, ClutSwap, SemiOn, SemiOff, FlatTint };
  ObjectEffect(Kind kind, std::uint32_t value) : kind_(kind), value_(value) {}

  Kind kind_ = Kind::None;
  std::uint32_t value_ = 0; // the node's colour word or CLUT
};

// Makes `effect` the one emission runs under until it ends, then restores the one before.
class ObjectEffectScope {
public:
  ObjectEffectScope(ObjectEffect &active, const ObjectEffect &effect) : active_(active), before_(active) {
    active_ = effect;
  }
  ~ObjectEffectScope() {
    active_ = before_;
  }
  ObjectEffectScope(const ObjectEffectScope &) = delete;
  ObjectEffectScope &operator=(const ObjectEffectScope &) = delete;
  ObjectEffectScope(ObjectEffectScope &&) = delete;
  ObjectEffectScope &operator=(ObjectEffectScope &&) = delete;

private:
  ObjectEffect &active_;
  ObjectEffect before_;
};

} // namespace tomba::render
