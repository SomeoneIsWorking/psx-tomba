#include "authenticated_image.h"
#include "core/overrides/native_override_catalog.h"
#include "game.h"
#include "lightrec_executor.h"
#include "render/guest_ordering_table.h"
#include "stub_runtime.h"

#include <cstdint>
#include <lucent/log.h>
#include <memory>
#include <optional>
#include <vector>

namespace {

constexpr std::uint32_t kModeSlot = 0x80108f9cu;
constexpr std::uint32_t kFileTable = 0x800be118u;
constexpr std::uint32_t kCollision = 0x801113b4u;
constexpr std::uint32_t kNode = 0x80012000u;
constexpr std::uint32_t kStack = 0x800e0000u;
constexpr std::uint32_t kReturn = 0x80010100u;
constexpr auto kBudget = psx::cpu::ExecutionBudget::fromCycles(200000u);
int a03Calls = 0;
int a0bCalls = 0;

void a03DiagnosticOwner(Core *core) {
  ++a03Calls;
  core->r[2] = 43u;
}

void a0bDiagnosticOwner(Core *core) {
  ++a0bCalls;
  core->r[2] = 47u;
}

bool check(bool condition, const char *name) {
  if (!condition) {
    lucent::error("real-overlay-test", "failed: {}", name);
  }
  return condition;
}

void copyMode(Core &core, const std::vector<std::uint8_t> &bytes, std::uint32_t fileIndex) {
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    core.mem_w8(kModeSlot + static_cast<std::uint32_t>(index), bytes[index]);
  }
  core.mem_w32(kFileTable + fileIndex * 8u + 4u, static_cast<std::uint32_t>(bytes.size()));
}

void prepareCall(Core &core) {
  core.r[4] = kNode;
  core.r[29] = kStack;
  core.r[31] = kReturn;
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 5) {
    lucent::error("real-overlay-test", "expected A03 path/digest and A0B path/digest from the manifest verifier");
    return 2;
  }
  const auto a03Bytes = tomba::test::readAuthenticatedImage(argv[1], argv[2], 0x200000u - (kModeSlot & 0x1fffffffu));
  const auto a0bBytes = tomba::test::readAuthenticatedImage(argv[3], argv[4], 0x200000u - (kModeSlot & 0x1fffffffu));
  if (!a03Bytes || !a0bBytes) {
    return 2;
  }

  tomba::test::StubRuntime runtime;
  psxport_install_game(runtime);
  auto game = std::make_unique<Game>();
  Core &core = game->core;
  tomba::native::declareOverlayOverride("A03", kCollision, "diagnostic A03", a03DiagnosticOwner);
  tomba::native::declareOverlayOverride("A0B", kCollision, "diagnostic A0B", a0bDiagnosticOwner);
  std::optional<psx::cpu::ImageIdentity> active;

  copyMode(core, *a03Bytes, 6u);
  const auto a03 = tomba::native::activateModeOverlay(core, active, 6u);
  prepareCall(core);
  const auto nativeA03 = psx::cpu::dispatchGuest(core, kCollision, kBudget);
  if (!check(nativeA03.returned() && a03Calls == 1 && a0bCalls == 0 && core.r[2] == 43u,
             "authentic A03 residency selects only its diagnostic owner")) {
    return 1;
  }
  // The guest's pool-capacity branch returns before drawing or calling resident
  // services. This proves the real A03 wrapper and callee's early exit only.
  constexpr std::uint32_t kPoolCursorAddress = tomba2::render::PacketPool::kCursor;
  constexpr std::uint32_t kPoolFullCursor = 0x000ffff0u;
  core.mem_w32(kPoolCursorAddress, kPoolFullCursor);
  prepareCall(core);
  const auto beforeA03 = core.lightrecExecutor().counters().executedBlocks;
  const auto originalA03 = psx::cpu::callOriginal(core, {a03, kCollision}, kBudget);
  if (!check(originalA03.returned() && core.r[29] == kStack && core.mem_r32(kPoolCursorAddress) == kPoolFullCursor &&
                 core.lightrecExecutor().counters().executedBlocks > beforeA03,
             "A03 scoped original executes its authenticated wrapper and capacity-exit guest body")) {
    return 1;
  }

  const auto beforeCopy = core.lightrecExecutor().counters().invalidations;
  copyMode(core, *a0bBytes, 14u);
  const auto copyInvalidations = core.lightrecExecutor().counters().invalidations - beforeCopy;
  const auto a0b = tomba::native::activateModeOverlay(core, active, 14u);
  prepareCall(core);
  const auto nativeA0B = psx::cpu::dispatchGuest(core, kCollision, kBudget);
  if (!check(nativeA0B.returned() && a03Calls == 1 && a0bCalls == 1 && core.r[2] == 47u &&
                 !core.nativeDispatcher().isInstalled({a03, kCollision}) && copyInvalidations > 0,
             "A0B byte copy invalidates translated blocks and replacement retires A03 dispatch")) {
    return 1;
  }
  core.mem_w8(kNode + 4u, 1u);
  core.mem_w8(kNode + 7u, 1u);
  prepareCall(core);
  const auto beforeA0B = core.lightrecExecutor().counters().executedBlocks;
  const auto originalA0B = psx::cpu::callOriginal(core, {a0b, kCollision}, kBudget);
  if (!check(originalA0B.returned() && core.mem_r8(kNode + 4u) == 2u && core.mem_r8(kNode + 7u) == 0u &&
                 core.r[29] == kStack && core.lightrecExecutor().counters().executedBlocks > beforeA0B,
             "A0B scoped original executes its authenticated timer-to-state-2 transition")) {
    return 1;
  }

  tomba::native::retireOverlay(core, active);
  const auto unrelated = tomba::native::activateOverlay(
      core,
      active,
      "unrelated",
      {kModeSlot & 0x1fffffffu, (kModeSlot & 0x1fffffffu) + static_cast<std::uint32_t>(a0bBytes->size())});
  core.mem_w8(kNode + 4u, 1u);
  core.mem_w8(kNode + 7u, 1u);
  prepareCall(core);
  const auto wrongImage = psx::cpu::dispatchGuest(core, kCollision, kBudget);
  if (!check(wrongImage.returned() && core.currentImageIdentity(kCollision) == unrelated && a0bCalls == 1 &&
                 core.mem_r8(kNode + 4u) == 2u && !core.nativeDispatcher().isInstalled({unrelated, kCollision}),
             "forced wrong-image negative executes guest bytes without either diagnostic owner")) {
    return 1;
  }

  const auto &counters = core.lightrecExecutor().counters();
  lucent::info("real-overlay-test",
               "PASS: A03/A0B authenticated collision; original exits=2; diagnostic owners={},{}, "
               "executed_blocks={} executed_instructions={} copy_invalidations={} invalidations={} "
               "fallback_blocks={}",
               a03Calls,
               a0bCalls,
               counters.executedBlocks,
               counters.executedInstructions,
               copyInvalidations,
               counters.invalidations,
               counters.fallback.calls);
  return counters.executedBlocks > 0 && counters.fallback.calls == 0 ? 0 : 1;
}
