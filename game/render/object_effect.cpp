// game/render/object_effect.cpp — ObjectEffect, the five effect leaves. See object_effect.h.
// Each leaf walks the span with the guest's 29-entry jump table on (cmd & 0xFC) - 0x20.
#include "object_effect.h"

#include "core.h"

#include <lucent/log.h>

#include <algorithm>
#include <cstdlib>

namespace tomba::render {
namespace {

constexpr std::uint32_t kCommandByte = 0x07u; // shares colour word 0
constexpr std::uint8_t kSemiBit = 0x02u;
constexpr std::uint32_t kClut = 0x0Eu;
constexpr std::uint32_t kColourWord[4] = {0x04u, 0x10u, 0x1Cu, 0x28u};

constexpr std::uint32_t kNodeColour = 0x18u;
constexpr std::uint32_t kNodeClut = 0x5Cu;

struct PacketShape {
  std::uint8_t stride;      // bytes to the next packet
  std::uint8_t colourWords; // 0: stepped over
};

PacketShape shapeOf(std::uint8_t opcode) {
  switch (opcode) {
  case 0x20u:
    return {20, 0};
  case 0x24u:
    return {32, 1};
  case 0x28u:
    return {24, 0};
  case 0x2Cu:
    return {40, 1};
  case 0x30u:
    return {28, 0};
  case 0x34u:
    return {40, 3};
  case 0x38u:
    return {36, 0};
  case 0x3Cu:
    return {52, 4};
  default:
    return {0, 0};
  }
}

// The amount at 0x80 writes nothing; below it darkens by (amount - 0x7F), above it brightens by amount.
void addChannel(const psx::present::EmitMemory &memory, std::uint32_t address, std::uint8_t amount) {
  if (amount == 0x80u) {
    return;
  }
  const std::int32_t channel = memory.mem_r8(address);
  const std::int32_t value = amount < 0x80u ? std::max(channel + static_cast<std::int32_t>(amount) - 0x7F, 0)
                                            : std::min(channel + amount, 255);
  memory.mem_w8(address, static_cast<std::uint8_t>(value));
}

} // namespace

ObjectEffect ObjectEffect::colorAdd(Core &core, std::uint32_t node) {
  return {Kind::ColorAdd, core.mem_r32(node + kNodeColour)};
}

ObjectEffect ObjectEffect::clutSwap(Core &core, std::uint32_t node) {
  return {Kind::ClutSwap, core.mem_r16(node + kNodeClut)};
}

ObjectEffect ObjectEffect::semi(bool on) {
  return {on ? Kind::SemiOn : Kind::SemiOff, 0};
}

ObjectEffect ObjectEffect::flatTint(Core &core, std::uint32_t node) {
  return {Kind::FlatTint, core.mem_r32(node + kNodeColour)};
}

void ObjectEffect::apply(const psx::present::EmitMemory &memory, std::uint32_t lo, std::uint32_t hi) const {
  if (kind_ == Kind::None) {
    return;
  }
  for (std::uint32_t packet = lo; packet < hi;) {
    const std::uint8_t command = memory.mem_r8(packet + kCommandByte);
    const PacketShape shape = shapeOf(command & 0xFCu);
    if (shape.stride == 0) {
      lucent::error("render",
                    "object effect {}: GP0 opcode 0x{:02X} at packet 0x{:08X} in [0x{:08X},0x{:08X}) is not a polygon",
                    static_cast<std::uint32_t>(kind_),
                    command & 0xFCu,
                    packet,
                    lo,
                    hi);
      std::abort();
    }
    if (shape.colourWords != 0) {
      const psx::present::PacketAmendScope amend(memory, packet);
      switch (kind_) {
      case Kind::ColorAdd:
        // The three channel bytes only: the command byte shares the word.
        for (int word = 0; word < shape.colourWords; word++) {
          for (std::uint32_t channel = 0; channel < 3; channel++) {
            addChannel(
                memory, packet + kColourWord[word] + channel, static_cast<std::uint8_t>(value_ >> (channel * 8)));
          }
        }
        break;
      case Kind::ClutSwap:
        memory.mem_w16(packet + kClut, static_cast<std::uint16_t>(value_));
        break;
      case Kind::SemiOn:
        memory.mem_w8(packet + kCommandByte, command | kSemiBit);
        break;
      case Kind::SemiOff:
        memory.mem_w8(packet + kCommandByte, command & static_cast<std::uint8_t>(~kSemiBit));
        break;
      case Kind::FlatTint:
        // The colour word covers the command byte, which comes back with semi-transparency set.
        for (int word = 0; word < shape.colourWords; word++) {
          memory.mem_w32(packet + kColourWord[word], value_);
        }
        memory.mem_w8(packet + kCommandByte, command | kSemiBit);
        break;
      case Kind::None:
        break;
      }
    }
    packet += shape.stride;
  }
}

} // namespace tomba::render
