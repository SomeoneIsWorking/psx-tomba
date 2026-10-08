#include "core/overrides/native_override_catalog.h"
#include "game.h"
#include "game_runtime.h"
#include "level_load.h"
#include "lightrec_executor.h"

#include <lucent/log.h>
#include <memory>

namespace {

class Runtime final : public GameRuntime {
public:
  void *createContext(Core &) override {
    return nullptr;
  }
  void destroyContext(void *) override {}
  void registerOverrides(Game &) override {}
  void bootInit(Core &) override {}
  RenderCapabilities renderCapabilities() const override {
    return RenderCapabilities::direct();
  }
  bool guestVramIsPicture(const Game &) const override {
    return false;
  }
};

constexpr std::uint32_t kEntry = 0x80010000u;
constexpr std::uint32_t kReturn = 0x80010100u;
constexpr std::uint32_t kStageEntry = 0x8010696cu;
constexpr std::uint32_t kModeEntry = 0x8010A8D4u;
constexpr std::uint32_t kAreaEntry = 0x8018BD30u;
constexpr std::uint32_t kModeFileTable = 0x800BE118u;
constexpr std::uint32_t kProducerEntry = 0x80010080u;
constexpr std::uint32_t kListObject = 0x80020000u;
constexpr std::uint32_t kRecordObject = 0x80020040u;
constexpr std::uint32_t kListPacket = 0x80030000u;
constexpr std::uint32_t kRecordPacket = 0x80030010u;
constexpr std::uint32_t kUnscopedPacket = 0x80030020u;
constexpr GuestAddressRange kResidentText{0x10000u, 0x10110u};
int nativeCalls = 0;
int modeCalls = 0;
int areaCalls = 0;

void nativeOwner(Core *core) {
  ++nativeCalls;
  core->r[2] = 41u;
}

void nativeA03(Core *core) {
  ++nativeCalls;
  core->r[2] = 43u;
}

void nativeA0B(Core *core) {
  ++nativeCalls;
  core->r[2] = 47u;
}

void nativeDemo(Core *core) {
  ++nativeCalls;
  core->r[2] = 53u;
}

void nativeSop(Core *core) {
  ++modeCalls;
  core->r[2] = 59u;
}

void nativeModeA03(Core *core) {
  ++modeCalls;
  core->r[2] = 61u;
}

void nativeOpn(Core *core) {
  ++areaCalls;
  core->r[2] = 67u;
}

void nativeCrd(Core *core) {
  ++areaCalls;
  core->r[2] = 71u;
}

// A list walker: one packet's command word under the call's own object, one under a per-record instance.
void nativeProducer(Core *core) {
  core->mem_w32(kListPacket + 4u, 0x01000000u);
  const auto record = core->emission.instance(kRecordObject);
  core->mem_w32(kRecordPacket + 4u, 0x02000000u);
  core->r[2] = 73u;
}

int failed = 0;
int checked = 0;

void check(bool condition, const char *name) {
  ++checked;
  if (!condition) {
    ++failed;
    lucent::error("native-catalog-test", "failed: {}", name);
  }
}

} // namespace

int main() {
  Runtime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  core.mem_w32(kEntry, 0x24020005u);      // addiu v0, zero, 5
  core.mem_w32(kEntry + 4u, 0x03e00008u); // jr ra
  core.mem_w32(kEntry + 8u, 0u);
  const auto first = core.imageCatalog().activate("resident", kResidentText, 1u);
  tomba::native::declareOverride(kEntry, "native-owner", nativeOwner);
  tomba::native::declareOverlayOverride("A03", kEntry, "a03-owner", nativeA03);
  tomba::native::declareOverlayOverride("A0B", kEntry, "a0b-owner", nativeA0B);
  tomba::native::declareOverlayOverride("DEMO", kStageEntry, "demo-owner", nativeDemo);
  tomba::native::declareOverlayOverride("SOP", kModeEntry, "sop-owner", nativeSop);
  tomba::native::declareOverlayOverride("A03", kModeEntry, "mode-a03-owner", nativeModeA03);
  tomba::native::declareOverlayOverride("OPN", kAreaEntry, "opn-owner", nativeOpn);
  tomba::native::declareOverlayOverride("CRD", kAreaEntry, "crd-owner", nativeCrd);
  tomba::native::declareOverride(
      kProducerEntry, "producer-owner", nativeProducer, psx::present::Producer{psx::present::Arg::A0});
  tomba::native::bindResident(core, first, kResidentText);
  const auto budget = psx::cpu::ExecutionBudget::fromCycles(1000u);

  core.r[4] = kListObject;
  core.r[31] = kReturn;
  auto produced = psx::cpu::dispatchGuest(core, kProducerEntry, budget);
  core.mem_w32(kUnscopedPacket + 4u, 0x03000000u);
  const psx::present::RecordKey listKey{kProducerEntry, kListObject, 0u, 0u};
  const psx::present::RecordKey recordKey{kProducerEntry, kRecordObject, 0u, 0u};
  check(produced.returned() && core.r[2] == 73u && core.nativeDispatcher().isProducer(kProducerEntry) &&
            !core.nativeDispatcher().isProducer(kEntry),
        "a catalog declaration with a producer installs a producer override");
  check(core.emission.identityFor(kListPacket) == listKey && core.emission.identityFor(kRecordPacket) == recordKey &&
            !core.emission.identityFor(kUnscopedPacket) && !core.emission.isOpen(),
        "producer stores are keyed by the a0 object and by each walked instance, others stay unkeyed");
  core.r[31] = kReturn;
  auto result = psx::cpu::dispatchGuest(core, kEntry, budget);
  check(result.returned() && core.r[2] == 41u && nativeCalls == 1, "first image selects native owner");

  const auto second = core.imageCatalog().activate("resident", kResidentText, 1u);
  check(second.generation != first.generation, "reloading advances the image generation");
  core.r[31] = kReturn;
  result = psx::cpu::dispatchGuest(core, kEntry, budget);
  check(result.returned() && core.r[2] == 5u && nativeCalls == 1,
        "stale image registration cannot select the native owner");

  tomba::native::bindResident(core, second, kResidentText);
  tomba::native::bindResident(core, second, kResidentText);
  core.r[31] = kReturn;
  result = psx::cpu::dispatchGuest(core, kEntry, budget);
  check(result.returned() && core.r[2] == 41u && nativeCalls == 2, "final image rebinding is effective and idempotent");

  core.r[31] = kReturn;
  result = psx::cpu::callOriginal(core, psx::cpu::NativeKey{second, kEntry}, budget);
  check(result.returned() && core.r[2] == 5u && nativeCalls == 2,
        "scoped original executes guest instructions without native recursion");

  std::optional<psx::cpu::ImageIdentity> activeOverlay;
  const auto a03 = tomba::native::activateOverlay(core, activeOverlay, "A03", kResidentText);
  core.r[31] = kReturn;
  result = psx::cpu::dispatchGuest(core, kEntry, budget);
  check(result.returned() && core.r[2] == 43u && nativeCalls == 3 && core.nativeDispatcher().isInstalled({a03, kEntry}),
        "A03 overlay selects only its image-scoped owner at the colliding address");
  core.r[31] = kReturn;
  result = psx::cpu::callOriginal(core, psx::cpu::NativeKey{a03, kEntry}, budget);
  check(result.returned() && core.r[2] == 5u && nativeCalls == 3, "A03 scoped original reaches the overlay guest body");

  core.mem_w32(kEntry, 0x2402000bu); // A0B has a different guest body at the same address.
  const auto a0b = tomba::native::activateOverlay(core, activeOverlay, "A0B", kResidentText);
  core.r[31] = kReturn;
  result = psx::cpu::dispatchGuest(core, kEntry, budget);
  check(result.returned() && core.r[2] == 47u && nativeCalls == 4 &&
            core.nativeDispatcher().isInstalled({a0b, kEntry}) && core.currentImageIdentity(kEntry) == a0b,
        "A0B replacement retires A03 residency and selects its own owner");
  check(!core.nativeDispatcher().isInstalled({a03, kEntry}),
        "A03 native registrations are removed when its MODE residency is retired");

  core.mem_w32(kEntry, 0x24020009u); // different image: addiu v0, zero, 9
  const auto other = core.imageCatalog().activate("different-image", kResidentText, 2u);
  tomba::native::bindResident(core, second, kResidentText);
  core.r[31] = kReturn;
  result = psx::cpu::dispatchGuest(core, kEntry, budget);
  check(result.returned() && core.r[2] == 9u && nativeCalls == 4 &&
            !core.nativeDispatcher().isInstalled({other, kEntry}),
        "colliding image stays JIT and cannot acquire a resident native declaration");

  core.mem_w32(kModeEntry, 0x2402000au);      // SOP body: addiu v0, zero, 10
  core.mem_w32(kModeEntry + 4u, 0x03e00008u); // jr ra
  core.mem_w32(kModeEntry + 8u, 0u);
  core.mem_w32(kModeFileTable + 2u * 8u + 4u, 0x2000u);
  core.mem_w32(kModeFileTable + 6u * 8u + 4u, 0x2000u);
  std::optional<psx::cpu::ImageIdentity> activeMode;
  const auto sop = tomba::native::activateModeOverlay(core, activeMode, 2u);
  core.r[31] = kReturn;
  result = psx::cpu::dispatchGuest(core, kModeEntry, budget);
  check(result.returned() && core.r[2] == 59u && modeCalls == 1 &&
            core.nativeDispatcher().isInstalled({sop, kModeEntry}),
        "SOP file index publishes MODE residency and selects its own native owner");
  core.r[31] = kReturn;
  result = psx::cpu::callOriginal(core, psx::cpu::NativeKey{sop, kModeEntry}, budget);
  check(result.returned() && core.r[2] == 10u && modeCalls == 1,
        "SOP scoped original executes its guest body through Lightrec");

  core.mem_w32(kModeEntry, 0x2402000cu); // A03 body: addiu v0, zero, 12
  const auto invalidationsBeforeMode = core.lightrecExecutor().counters().invalidations;
  const auto modeA03 = tomba::native::activateModeOverlay(core, activeMode, 6u);
  core.r[31] = kReturn;
  result = psx::cpu::dispatchGuest(core, kModeEntry, budget);
  check(result.returned() && core.r[2] == 61u && modeCalls == 2 && core.currentImageIdentity(kModeEntry) == modeA03 &&
            !core.nativeDispatcher().isInstalled({sop, kModeEntry}) &&
            core.lightrecExecutor().counters().invalidations > invalidationsBeforeMode,
        "A03 file index replaces SOP identity and invalidates its native and translated body");
  core.r[31] = kReturn;
  result = psx::cpu::callOriginal(core, psx::cpu::NativeKey{modeA03, kModeEntry}, budget);
  check(result.returned() && core.r[2] == 12u && modeCalls == 2,
        "A03 scoped original executes its replacement guest body");

  core.mem_w32(kAreaEntry, 0x2402000eu);      // OPN body: addiu v0, zero, 14
  core.mem_w32(kAreaEntry + 4u, 0x03e00008u); // jr ra
  core.mem_w32(kAreaEntry + 8u, 0u);
  core.mem_w32(kModeFileTable + 0u * 8u + 4u, 0x4000u);
  core.mem_w32(kModeFileTable + 1u * 8u + 4u, 0x4000u);
  std::optional<psx::cpu::ImageIdentity> activeArea;
  const auto opn = tomba::native::activateAreaSlotOverlay(core, activeArea, 0u);
  core.r[31] = kReturn;
  result = psx::cpu::dispatchGuest(core, kAreaEntry, budget);
  check(result.returned() && core.r[2] == 67u && areaCalls == 1 &&
            core.nativeDispatcher().isInstalled({opn, kAreaEntry}),
        "OPN load publishes AREA residency and selects only its own native owner");
  core.r[31] = kReturn;
  result = psx::cpu::callOriginal(core, psx::cpu::NativeKey{opn, kAreaEntry}, budget);
  check(result.returned() && core.r[2] == 14u && areaCalls == 1,
        "OPN scoped original executes its guest body through Lightrec");

  core.mem_w32(kAreaEntry, 0x24020010u); // CRD body: addiu v0, zero, 16
  const auto invalidationsBeforeArea = core.lightrecExecutor().counters().invalidations;
  const auto crd = tomba::native::activateAreaSlotOverlay(core, activeArea, 1u);
  core.r[31] = kReturn;
  result = psx::cpu::dispatchGuest(core, kAreaEntry, budget);
  check(result.returned() && core.r[2] == 71u && areaCalls == 2 && core.currentImageIdentity(kAreaEntry) == crd &&
            !core.nativeDispatcher().isInstalled({opn, kAreaEntry}) &&
            core.lightrecExecutor().counters().invalidations > invalidationsBeforeArea,
        "CRD replacement retires OPN's native owner and translated body");
  tomba::native::retireOverlay(core, activeArea);
  check(!activeArea && !core.currentImageIdentity(kAreaEntry),
        "raw area-data overwrite retires the prior executable AREA identity");

  core.mem_w32(kStageEntry, 0x24020007u);      // addiu v0, zero, 7
  core.mem_w32(kStageEntry + 4u, 0x03e00008u); // jr ra
  core.mem_w32(kStageEntry + 8u, 0u);
  std::optional<psx::cpu::ImageIdentity> activeStage;
  const auto start = tomba::stage::activateLoaded(core, activeStage, 0u, 2048u);
  core.r[31] = kReturn;
  result = psx::cpu::dispatchGuest(core, kStageEntry, budget);
  check(result.returned() && core.r[2] == 7u && nativeCalls == 4 &&
            !core.nativeDispatcher().isInstalled({start, kStageEntry}),
        "START stage cannot acquire DEMO's native declaration");

  const auto demo = tomba::stage::activateLoaded(core, activeStage, 1u, 2048u);
  core.r[31] = kReturn;
  result = psx::cpu::dispatchGuest(core, kStageEntry, budget);
  check(result.returned() && core.r[2] == 53u && nativeCalls == 5 &&
            core.nativeDispatcher().isInstalled({demo, kStageEntry}),
        "DEMO stage binds its own native declaration after START replacement");
  core.r[31] = kReturn;
  result = psx::cpu::callOriginal(core, psx::cpu::NativeKey{demo, kStageEntry}, budget);
  check(result.returned() && core.r[2] == 7u && nativeCalls == 5,
        "DEMO scoped original executes its loaded guest body");

  core.mem_w32(kStageEntry, 0x2402000du); // GAME body: addiu v0, zero, 13
  const auto invalidationsBeforeGame = core.lightrecExecutor().counters().invalidations;
  const auto gameStage = tomba::stage::activateLoaded(core, activeStage, 2u, 2048u);
  core.r[31] = kReturn;
  result = psx::cpu::dispatchGuest(core, kStageEntry, budget);
  check(result.returned() && core.r[2] == 13u && nativeCalls == 5 &&
            core.currentImageIdentity(kStageEntry) == gameStage &&
            !core.nativeDispatcher().isInstalled({demo, kStageEntry}) &&
            core.lightrecExecutor().counters().invalidations > invalidationsBeforeGame,
        "GAME replacement invalidates the DEMO dispatch and translated body");
  tomba::native::retireOverlay(core, activeStage);
  check(!activeStage && !core.currentImageIdentity(kStageEntry), "resident stage retires the previous code image");
  constexpr std::uint32_t kTaskFields = 0x1f800040u;
  constexpr std::uint32_t kResidentEntry = 0x80045678u;
  constexpr std::uint32_t kCallerGp = 0x80012345u;
  core.mem_w32(tomba::stage::kEntryTable + 3u * 4u, kResidentEntry);
  core.r[28] = kCallerGp;
  tomba::stage::loadOverlay(core, activeStage, kTaskFields, 3u);
  check(core.mem_r32(kTaskFields) == kResidentEntry && core.mem_r32(kTaskFields + 4u) == kCallerGp && !activeStage,
        "resident stage writes distinct entry PC and caller gp to the task context");
  const auto &counters = core.lightrecExecutor().counters();
  check(counters.executedBlocks > 0 && counters.fallback.calls == 0,
        "original calls execute translated blocks without interpreter fallback");
  lucent::info("native-catalog-test", "checked={} failed={}", checked, failed);
  return failed == 0 ? 0 : 1;
}
