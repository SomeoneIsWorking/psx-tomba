#include "scene/continue_save.h"

#include "card_load_machine.h"
#include "core.h"
#include "game.h"
#include "guest_call.h"

#include <lucent/log.h>

#include <array>
#include <cstdio>
#include <cstdlib>

namespace tomba::scene {

namespace {

constexpr std::uint8_t kFiles = 15;
constexpr std::uint32_t kImage = 0x380u;   // file offset of the state image
constexpr std::uint32_t kPlayTime = 0x08u; // image u32, frames played
constexpr std::uint32_t kCardMenuStep = 0x8007BE18u;
constexpr std::uint32_t kOpenRead = 0x8018B020u; // CardLoad_openRead
constexpr std::uint32_t kVerifyApply = 0x8018B1A4u;
constexpr std::uint32_t kBrowser = 0x800BF498u;    // g_CardBrowser
constexpr std::uint32_t kListNumber = 0x800E7ADCu; // the list buffer's first entry, its file number
// g_CardBrowser fields.
constexpr std::uint32_t kResult = 0x3Eu; // 0 busy, 1 ok, else failed
constexpr std::uint32_t kOp = 0x3Fu;
constexpr std::uint32_t kPort = 0x46u;
constexpr std::uint32_t kListIndex = 0x47u;
constexpr std::uint32_t kLastNumber = 0x48u; // + port
constexpr std::uint32_t kOpDelay = 0x51u;
constexpr std::uint32_t kRetries = 0x5Au;
constexpr std::uint8_t kOpenDue = 7; // openRead runs once the delay has passed 6
constexpr std::uint8_t kRetryOpen = 2;
constexpr std::uint8_t kVerify = 3;
constexpr std::uint32_t kPageAnswer = 0x50u; // sm, CardMenu_step's switch

std::array<char, 24> fileName(std::uint8_t number) {
  std::array<char, 24> name{};
  std::snprintf(name.data(), name.size(), "BASCUSP94454T2-%02u", static_cast<unsigned>(number));
  return name;
}

} // namespace

std::optional<std::uint8_t> ContinueSave::newest(Core &core) {
  Memcard &card = core.game->memcard;
  std::optional<std::uint8_t> best;
  std::uint32_t bestTime = 0;
  for (std::uint8_t number = 0; number < kFiles; ++number) {
    const int block = card.dirFind(fileName(number).data());
    if (block < 0) {
      continue;
    }
    std::array<std::uint8_t, 4> time{};
    if (!card.readFile(block, kImage + kPlayTime, time)) {
      continue;
    }
    const std::uint32_t frames =
        time[0] | (time[1] << 8) | (time[2] << 16) | (static_cast<std::uint32_t>(time[3]) << 24);
    if (!best || frames > bestTime) {
      best = number;
      bestTime = frames;
    }
  }
  return best;
}

bool ContinueSave::load(Core &core, std::uint8_t number) {
  Core *c = &core;
  loadCardCode(core);
  const std::uint32_t sm = c->mem_r32(kStatePtr);
  c->mem_w16(sm + kPageAnswer, 0);
  psx::cpu::callGuestNow(*c, __func__, kCardMenuStep, 0u); // CardBrowser_init(0)
  c->mem_w16(sm + kPageAnswer, 0);

  c->mem_w8(kBrowser + kPort, 0);
  c->mem_w8(kBrowser + kListIndex, 0);
  c->mem_w8(kListNumber, number);
  c->mem_w8(kBrowser + kRetries, 0);
  c->mem_w8(kBrowser + kResult, 0);
  // openRead re-arms itself on a failed open and gives up after seven; verify re-arms it on a new card.
  while (c->mem_r8(kBrowser + kResult) == 0u) {
    c->mem_w8(kBrowser + kOpDelay, kOpenDue);
    psx::cpu::callGuestNow(*c, __func__, kOpenRead, kBrowser);
    if (c->mem_r8(kBrowser + kResult) != 0u || c->mem_r8(kBrowser + kOp) != kVerify) {
      continue;
    }
    psx::cpu::callGuestNow(*c, __func__, kVerifyApply, kBrowser);
    // The card answers at once, so a verify that neither settles nor re-opens is a lost event.
    if (c->mem_r8(kBrowser + kResult) == 0u && c->mem_r8(kBrowser + kOp) != kRetryOpen) {
      lucent::error("continue", "card load of file {} raised no event", number);
      std::abort();
    }
  }

  const bool loaded = c->mem_r8(kBrowser + kResult) == 1u;
  if (loaded) {
    c->mem_w8(kBrowser + kLastNumber, number);
  }
  lucent::info("continue", "file {} {}", number, loaded ? "loaded" : "refused");
  return loaded;
}

} // namespace tomba::scene
