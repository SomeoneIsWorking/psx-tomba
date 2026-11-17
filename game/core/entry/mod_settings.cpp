#include "entry/mod_settings.h"

#include "config_var.h"
#include "enhancement_gate.h"
#include "items/suit_items.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace tomba {
namespace {

psx::config::Var<long> cvPitVoidOut("PSXPORT_TOMBA2_PIT_VOID_OUT", 0, psx::config::kSaved);
psx::config::Var<long> cvBirdFlutter("PSXPORT_TOMBA2_SUIT_BIRD_FLUTTER", 0, psx::config::kSaved);
psx::config::Var<long> cvBirdAirControl("PSXPORT_TOMBA2_SUIT_BIRD_AIR_CONTROL", 0, psx::config::kSaved);
psx::config::Var<long> cvSquirrelGlide("PSXPORT_TOMBA2_SUIT_SQUIRREL_GLIDE", 0, psx::config::kSaved);
psx::config::Var<long> cvSquirrelAirControl("PSXPORT_TOMBA2_SUIT_SQUIRREL_AIR_CONTROL", 0, psx::config::kSaved);
psx::config::Var<long> cvSquirrelCold("PSXPORT_TOMBA2_SUIT_SQUIRREL_COLD", 0, psx::config::kSaved);
psx::config::Var<long> cvPigJump("PSXPORT_TOMBA2_SUIT_PIG_JUMP", 0, psx::config::kSaved);
psx::config::Var<long> cvPigStomp("PSXPORT_TOMBA2_SUIT_PIG_STOMP", 0, psx::config::kSaved);
psx::config::Var<long> cvSwimmingPigSwim("PSXPORT_TOMBA2_SUIT_SWIMMING_PIG_SWIM", 0, psx::config::kSaved);
psx::config::Var<long> cvSwimmingPigJump("PSXPORT_TOMBA2_SUIT_SWIMMING_PIG_JUMP", 0, psx::config::kSaved);
psx::config::Var<long> cvSwimmingPigStomp("PSXPORT_TOMBA2_SUIT_SWIMMING_PIG_STOMP", 0, psx::config::kSaved);
psx::config::Var<long> cvFlameRobeCold("PSXPORT_TOMBA2_SUIT_FLAME_ROBE_COLD", 0, psx::config::kSaved);
psx::config::Var<long> cvSnowGrip("PSXPORT_TOMBA2_SUIT_SNOW_GRIP", 0, psx::config::kSaved);
psx::config::Var<long> cvFreezeGuard("PSXPORT_TOMBA2_SUIT_FREEZE_GUARD", 0, psx::config::kSaved);
psx::config::Var<long> cvSquirrelUpdraft("PSXPORT_TOMBA2_SUIT_SQUIRREL_UPDRAFT", 0, psx::config::kSaved);
psx::config::Var<long> cvFlameRobeFreeze("PSXPORT_TOMBA2_SUIT_FLAME_ROBE_FREEZE", 0, psx::config::kSaved);
const psx::config::Registration kRegistration{cvPitVoidOut,
                                              cvBirdFlutter,
                                              cvBirdAirControl,
                                              cvSquirrelGlide,
                                              cvSquirrelAirControl,
                                              cvSquirrelCold,
                                              cvPigJump,
                                              cvPigStomp,
                                              cvSwimmingPigSwim,
                                              cvSwimmingPigJump,
                                              cvSwimmingPigStomp,
                                              cvFlameRobeCold,
                                              cvSnowGrip,
                                              cvFreezeGuard,
                                              cvSquirrelUpdraft,
                                              cvFlameRobeFreeze};

// In SuitEffect order.
const std::array<psx::config::Var<long> *, 15> kSuitEffects{
    &cvBirdFlutter,
    &cvBirdAirControl,
    &cvSquirrelGlide,
    &cvSquirrelAirControl,
    &cvSquirrelCold,
    &cvPigJump,
    &cvPigStomp,
    &cvSwimmingPigSwim,
    &cvSwimmingPigJump,
    &cvSwimmingPigStomp,
    &cvFlameRobeCold,
    &cvSnowGrip,
    &cvFreezeGuard,
    &cvSquirrelUpdraft,
    &cvFlameRobeFreeze,
};

const std::array<const char *, 2> kOffOn{"Off", "On"};
constexpr const char *kBirdRow = "Bird Clothes";
constexpr const char *kSquirrelRow = "Squirrel Clothes";
constexpr const char *kPigRow = "Pig Suit";
constexpr const char *kSwimmingPigRow = "Swimming Pig Suit";
constexpr const char *kFlameRobeRow = "Evil Flame Pig Robe";
constexpr const char *kHeavierRow = "Any heavier suit";
// One row per suit, each effect a part of it; an owned suit lends the effects that are on.
const std::array<psx::game::TitleIntSetting, 16> kTitleIntSettings{{
    {.id = "pit_void_out",
     .label = "Pits void out like water",
     .unit = "",
     .var = &cvPitVoidOut,
     .range = {.min = 0, .max = 1, .step = 1},
     .labels = kOffOn},
    {.id = "suit_bird_flutter",
     .label = "Flutter",
     .unit = "",
     .var = &cvBirdFlutter,
     .range = {.min = 0, .max = 1, .step = 1},
     .labels = kOffOn,
     .row = kBirdRow},
    {.id = "suit_bird_air_control",
     .label = "Air control",
     .unit = "",
     .var = &cvBirdAirControl,
     .range = {.min = 0, .max = 1, .step = 1},
     .labels = kOffOn,
     .row = kBirdRow},
    {.id = "suit_squirrel_glide",
     .label = "Glide",
     .unit = "",
     .var = &cvSquirrelGlide,
     .range = {.min = 0, .max = 1, .step = 1},
     .labels = kOffOn,
     .row = kSquirrelRow},
    {.id = "suit_squirrel_air_control",
     .label = "Air control",
     .unit = "",
     .var = &cvSquirrelAirControl,
     .range = {.min = 0, .max = 1, .step = 1},
     .labels = kOffOn,
     .row = kSquirrelRow},
    {.id = "suit_squirrel_cold",
     .label = "Cold immunity",
     .unit = "",
     .var = &cvSquirrelCold,
     .range = {.min = 0, .max = 1, .step = 1},
     .labels = kOffOn,
     .row = kSquirrelRow},
    {.id = "suit_squirrel_updraft",
     .label = "Updraft",
     .unit = "",
     .var = &cvSquirrelUpdraft,
     .range = {.min = 0, .max = 1, .step = 1},
     .labels = kOffOn,
     .row = kSquirrelRow},
    {.id = "suit_pig_jump",
     .label = "Jump",
     .unit = "",
     .var = &cvPigJump,
     .range = {.min = 0, .max = 1, .step = 1},
     .labels = kOffOn,
     .row = kPigRow},
    {.id = "suit_pig_stomp",
     .label = "Stomp",
     .unit = "",
     .var = &cvPigStomp,
     .range = {.min = 0, .max = 1, .step = 1},
     .labels = kOffOn,
     .row = kPigRow},
    {.id = "suit_swimming_pig_swim",
     .label = "Swim",
     .unit = "",
     .var = &cvSwimmingPigSwim,
     .range = {.min = 0, .max = 1, .step = 1},
     .labels = kOffOn,
     .row = kSwimmingPigRow},
    {.id = "suit_swimming_pig_jump",
     .label = "Jump",
     .unit = "",
     .var = &cvSwimmingPigJump,
     .range = {.min = 0, .max = 1, .step = 1},
     .labels = kOffOn,
     .row = kSwimmingPigRow},
    {.id = "suit_swimming_pig_stomp",
     .label = "Stomp",
     .unit = "",
     .var = &cvSwimmingPigStomp,
     .range = {.min = 0, .max = 1, .step = 1},
     .labels = kOffOn,
     .row = kSwimmingPigRow},
    {.id = "suit_flame_robe_cold",
     .label = "Cold immunity",
     .unit = "",
     .var = &cvFlameRobeCold,
     .range = {.min = 0, .max = 1, .step = 1},
     .labels = kOffOn,
     .row = kFlameRobeRow},
    {.id = "suit_flame_robe_freeze",
     .label = "Freeze immunity",
     .unit = "",
     .var = &cvFlameRobeFreeze,
     .range = {.min = 0, .max = 1, .step = 1},
     .labels = kOffOn,
     .row = kFlameRobeRow},
    {.id = "suit_snow_grip",
     .label = "Snow grip",
     .unit = "",
     .var = &cvSnowGrip,
     .range = {.min = 0, .max = 1, .step = 1},
     .labels = kOffOn,
     .row = kHeavierRow},
    {.id = "suit_freeze_guard",
     .label = "Freeze guard",
     .unit = "",
     .var = &cvFreezeGuard,
     .range = {.min = 0, .max = 1, .step = 1},
     .labels = kOffOn,
     .row = kHeavierRow},
}};

// The row whose effects an owned item lends.
struct ItemRow {
  std::uint8_t item;
  const char *row;
};
constexpr std::array<ItemRow, 5> kItemRows{{
    {suit_items::kBird, kBirdRow},
    {suit_items::kSquirrel, kSquirrelRow},
    {suit_items::kPig, kPigRow},
    {suit_items::kSwimmingPig, kSwimmingPigRow},
    {suit_items::kFlameRobe, kFlameRobeRow},
}};

std::span<const psx::game::TitleIntSetting> rowNamed(std::string_view row) {
  for (std::size_t i = 0; i < kTitleIntSettings.size(); ++i) {
    if (kTitleIntSettings[i].row != nullptr && row == kTitleIntSettings[i].row) {
      return psx::game::titleSettingRowAt(kTitleIntSettings, i).parts;
    }
  }
  return {};
}

} // namespace

bool ModSettings::pitVoidOut() {
  return psx::config::enh_int(cvPitVoidOut) != 0;
}

bool ModSettings::suitEffect(SuitEffect effect) {
  return psx::config::enh_int(*kSuitEffects[static_cast<std::size_t>(effect)]) != 0;
}

std::span<const psx::game::TitleIntSetting> ModSettings::titleIntSettings() {
  return kTitleIntSettings;
}

std::span<const psx::game::TitleIntSetting> ModSettings::suitEffects(std::uint8_t item) {
  for (const ItemRow &entry : kItemRows) {
    if (entry.item == item) {
      return rowNamed(entry.row);
    }
  }
  return {};
}

std::span<const psx::game::TitleIntSetting> ModSettings::heavierSuitEffects(std::uint8_t item) {
  return suit_items::heavier(item) ? rowNamed(kHeavierRow) : std::span<const psx::game::TitleIntSetting>();
}

} // namespace tomba
