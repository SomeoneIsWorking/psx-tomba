#include "entry/mod_settings.h"

#include "config_var.h"
#include "enhancement_gate.h"

#include <array>
#include <cstddef>

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
const std::array<psx::game::TitleIntSetting, 16> kTitleIntSettings{{
    {.id = "pit_void_out",
     .label = "Pits void out like water",
     .unit = "",
     .var = &cvPitVoidOut,
     .range = {.min = 0, .max = 1, .step = 1},
     .labels = kOffOn},
    {.id = "suit_bird_flutter",
     .label = "Owned Bird Clothes: flutter",
     .unit = "",
     .var = &cvBirdFlutter,
     .range = {.min = 0, .max = 1, .step = 1},
     .labels = kOffOn},
    {.id = "suit_bird_air_control",
     .label = "Owned Bird Clothes: air control",
     .unit = "",
     .var = &cvBirdAirControl,
     .range = {.min = 0, .max = 1, .step = 1},
     .labels = kOffOn},
    {.id = "suit_squirrel_glide",
     .label = "Owned Squirrel Clothes: glide",
     .unit = "",
     .var = &cvSquirrelGlide,
     .range = {.min = 0, .max = 1, .step = 1},
     .labels = kOffOn},
    {.id = "suit_squirrel_air_control",
     .label = "Owned Squirrel Clothes: air control",
     .unit = "",
     .var = &cvSquirrelAirControl,
     .range = {.min = 0, .max = 1, .step = 1},
     .labels = kOffOn},
    {.id = "suit_squirrel_cold",
     .label = "Owned Squirrel Clothes: cold immunity",
     .unit = "",
     .var = &cvSquirrelCold,
     .range = {.min = 0, .max = 1, .step = 1},
     .labels = kOffOn},
    {.id = "suit_pig_jump",
     .label = "Owned Pig Suit: high jump",
     .unit = "",
     .var = &cvPigJump,
     .range = {.min = 0, .max = 1, .step = 1},
     .labels = kOffOn},
    {.id = "suit_pig_stomp",
     .label = "Owned Pig Suit: stomp",
     .unit = "",
     .var = &cvPigStomp,
     .range = {.min = 0, .max = 1, .step = 1},
     .labels = kOffOn},
    {.id = "suit_swimming_pig_swim",
     .label = "Owned Swimming Pig Suit: swim",
     .unit = "",
     .var = &cvSwimmingPigSwim,
     .range = {.min = 0, .max = 1, .step = 1},
     .labels = kOffOn},
    {.id = "suit_swimming_pig_jump",
     .label = "Owned Swimming Pig Suit: high jump",
     .unit = "",
     .var = &cvSwimmingPigJump,
     .range = {.min = 0, .max = 1, .step = 1},
     .labels = kOffOn},
    {.id = "suit_swimming_pig_stomp",
     .label = "Owned Swimming Pig Suit: stomp",
     .unit = "",
     .var = &cvSwimmingPigStomp,
     .range = {.min = 0, .max = 1, .step = 1},
     .labels = kOffOn},
    {.id = "suit_flame_robe_cold",
     .label = "Owned Evil Flame Pig Robe: cold immunity",
     .unit = "",
     .var = &cvFlameRobeCold,
     .range = {.min = 0, .max = 1, .step = 1},
     .labels = kOffOn},
    {.id = "suit_snow_grip",
     .label = "Owned suits grip on snow",
     .unit = "",
     .var = &cvSnowGrip,
     .range = {.min = 0, .max = 1, .step = 1},
     .labels = kOffOn},
    {.id = "suit_freeze_guard",
     .label = "Owned suits resist the ice monster's freeze",
     .unit = "",
     .var = &cvFreezeGuard,
     .range = {.min = 0, .max = 1, .step = 1},
     .labels = kOffOn},
    {.id = "suit_squirrel_updraft",
     .label = "Owned Squirrel Clothes: updraft reach",
     .unit = "",
     .var = &cvSquirrelUpdraft,
     .range = {.min = 0, .max = 1, .step = 1},
     .labels = kOffOn},
    {.id = "suit_flame_robe_freeze",
     .label = "Owned Evil Flame Pig Robe: freeze immunity",
     .unit = "",
     .var = &cvFlameRobeFreeze,
     .range = {.min = 0, .max = 1, .step = 1},
     .labels = kOffOn},
}};

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

} // namespace tomba
