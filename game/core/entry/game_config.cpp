// game_config.cpp — measured Tomba!2 compatibility facts.
//
// TombaRuntime is the title's ownership seam. This legacy GameConfig remains only because generic
// psxport algorithms still read `c->cfg->field`; each typed framework extraction must delete its
// corresponding fields here rather than growing this bag. Every number preserves a measured
// MAIN.EXE fact that the framework previously baked in directly.
//
// DESIGNATED INITIALIZERS, deliberately (converted 2026-07-29). This used to be a POSITIONAL
// initializer whose fields were labelled by `/* name */` comments — labels the compiler never
// checked. game_iface.h says outright that "GameConfig is initialised POSITIONALLY by every
// consumer, so inserting a field mid-struct silently shifts every value after it", and has been
// appending new fields at the ends of groups to work around exactly that. Appending is a discipline
// the framework has to remember on every change; a designator is checked on every build. With these
// every field is bound by NAME, so a framework insertion anywhere can no longer slide this game's
// guest addresses one slot sideways — and a renamed or removed field becomes a compile error rather
// than a wrong address that boots and diverges. Fields left unset are value-initialised to zero,
// which is the framework's documented "this game has no such primitive".
#include "frame/frame_cadence.h"
#include "game_iface.h"
#include "render/ordering_table.h"
#include "title_facts.h"

// Task entry PCs, verbatim from the literals psxport's pc_scheduler.cpp used to carry. Values unchanged:
// this is a MOVE of a declaration, not a re-measurement, so the port's scheduling must be identical and
// the boot gate is what proves it (stage=8010637C sm48=2, measured before and after the move).
static const GameConfig::SchedEntry g_tomba_sched_entries[] = {
    // pc            nativeHandler  hasFiberBody  fiberBody
    {0x801062E4u, 1, 0, SCHED_DEMO_STAGEMAIN},         // DEMO dispatcher
    {0x8010637Cu, 1, 0, SCHED_GAME_PROLOGUE},          // GAME dispatcher
    {0x8010649Cu, 1, 0, SCHED_FIBER_STARTBIN},         // STAGE-0 START.BIN
    {0x80109164u, 1, 1, SCHED_CORO_AREALOAD_FAITHFUL}, // SOP area-load
    {0x80044F58u, 0, 1, SCHED_CORO_TEXGROUP},          // preload body
    {0x8004514Cu, 0, 1, SCHED_CORO_PRELOAD1},          // stage-1 callback
    {0x800452C0u, 0, 1, SCHED_CORO_AREADATA},          // walkable-field area DATA
};

static const GameConfig g_tomba_config = {
    // --- crt0 / boot (native_boot.cpp crt0_setup, game_init) ---
    .bssZeroLo = 0x800be0d8u,
    .bssZeroHi = 0x80106228u,
    .stackTopBase = 0x800a3f88u,
    .stackTopBase2 = 0x800a3f8cu,
    .heapBase = 0x80106228u,
    .heapSizePtr = 0x800abef8u,
    .heapBasePtr = 0x800abef4u,
    .gp = 0x800be0d4u,
    .libcInit = 0x80089860u,
    .gameMain = 0x80050b08u, // FUN_80050b08 (native-overridden game-main; comment-only literal)
    .crt0 = 0x800896e0u,     // FUN_800896E0 (native crt0; comment-only literal)

    // Guest MAIN.EXE text range (physical, address & 0x1FFFFFFF), measured from the executable
    // image. These title facts are runtime image bounds, not source-generation inputs.
    .recMainLo = 0x00010000u,
    .recMainHi = 0x000BE800u,

    // Name of the environment variable / .env key that points at THIS game's disc image. The disc
    // resolver in disc.c used to hardcode this string; it now reads it from here, so a second consumer
    // can set its own key instead of silently booting with no media.
    .discEnvVar = "PSXPORT_TOMBA2_DISC",

    // Boot intro movies, in play order. native_boot_run used to hardcode this path; it now reads it
    // from here so a second consumer can name its own (or none). Only LOGO.STR belongs at boot —
    // OP.STR is the front-end's, and playing it here too caused the "FMV repeats" bug.
    .bootFmv = {"MOVIE/LOGO.STR", nullptr, nullptr, nullptr},

    // --- per-frame OT / packet-pool dance (native_boot.cpp native_step_frame) ---
    .otRegionBase = 0x800e80a8u,
    .otRegionStride = 0x00002070u,
    .packetPoolBase = 0x800bfe68u,
    .packetPoolStride = 0x00014000u,
    .otBasePtr = tomba2::render::OrderingTable::kBasePointer,
    .dwellCounter = tomba::FrameCadence::kDwellCounterAddress,
    .poolPtrCur = tomba2::render::PacketPool::kCursor,
    .poolPtrLast = 0x800bf4f4u,
    .clearOtagR = 0x80081458u,
    .putDrawEnv = 0x800815d0u,
    .drawSync = 0x80080f6cu,
    .irqEventClasses = {0xF2000003u, 0xF0000001u, 0xF0000009u},

    // --- scheduler task layout (scheduler.cpp, native_boot probes) ---
    .taskTableBase = 0x801fe000u,
    .taskSlotStride = 0x00000070u,
    .taskCount = 3u, // up to 3 cooperative tasks (loop lives game-side, no framework literal)
    .curTaskPtr = 0x1f800138u,
    .stageStart = 0x8010649cu,
    .stageDemo = 0x801062e4u,
    .stageGame = 0x8010637cu,
    // --- overlay router slots (overlay_router.cpp slot_index) ---
    .overlaySlots =
        {
            {0x80106228u, "STAGE"}, // START/DEMO/GAME
            {0x80108f9cu, "MODE"},  // SOP / A0* field area code
            {0x8018a000u, "AREA"},  // OPN; also raw area DATA
        },

    // --- CD chokepoints (cd_override.cpp) ---
    .cdInit = 0x8008b2d8u, // CdInit handshake (registered by PlatformHle::initBuiltins, not cd_override)
    .cdCommand = 0x8008ac34u,
    .cdSync = 0x8008a6ecu,
    .cdReadPrim = 0x8008c1ecu,
    .cdFileLoad = 0x8001db8cu,
    .cdAsyncRead = 0x8001d940u,
    .voicePlay = 0x8001d2a8u,
    .voiceStop = 0x8001cf2cu,
    .lastSectorTracker = 0x800be0e0u,
    .cdInlineLoad = 0x8001dc40u,
    .cdCmdStream = 0x8001ce90u,
    .cdCallbackTable = {0x800abfbcu, 0x800abfc0u, 0x800abf24u, 0x800abf28u},
    .cdCallbackFn = {0x8009996cu, 0x80089994u, 0x800899bcu, 0x00000000u},

    // STOCK Sony libcd entry points — ALL ZERO FOR THIS GAME, and that is a statement, not a gap.
    // Tomba!2 does not read through stock libcd: it drives the drive through its own engine loader
    // (cdFileLoad / cdAsyncRead / cdInlineLoad / cdCmdStream above, all FUN_8001xxxx engine code), so
    // there is no CdRead/CdReadSync/CdSearchFile/CdGetSector call site to intercept and no libcd
    // ready-callback, last-position buffer or DMA-completion slot to inherit. A game that DOES use
    // stock libcd fills these in; leaving them zero here keeps every one of those handlers unarmed.
    .cdGetSector = 0u,
    .cdReadyCbPtr = 0u,
    .cdLastPosBuf = 0u,
    .cdReadStock = 0u,
    .cdReadSync = 0u,
    .cdSearchFile = 0u,

    // --- DMA completion callback table (libapi DMACallback) ---
    // Zero: this game's DMACallback table has not been reverse-engineered, so NO channel completion is
    // dispatched — the same behaviour as a guest that registered none, never a wrong one. Was the
    // CD-only `cdDmaDoneCbPtr` until psxport 3f6a1e14 widened it to every channel; the value was 0 then
    // and is 0 now, so nothing about this port's dispatch changes.
    .dmaCallbackTable = 0u,

    // --- pad driver (pad_input.cpp) ---
    .padSlot0Buf = 0x800bf4f8u,
    .padSlot1Buf = 0x800bf51au,
    .padDriverFn = 0x80003a4cu, // FUN_80003A4C SIO pad read (inert: driver not in MAIN.EXE; no live register)
    .padSlotPtrTable = 0x0000aec8u,
    // Byte distance between consecutive slots' buffer pointers. This driver keeps a FLAT pointer
    // array, so 4. (0 would be read as 4 as well, but say it rather than lean on the fallback.)
    .padSlotPtrStride = 4u,

    // --- platform HLE: the PSX hardware-sync primitives (framework: sync_overrides.cpp) ---
    // These were hardcoded in the framework's initBuiltins() until 2026-07-28. They are facts about
    // MAIN.EXE, so they belong here — same move the seed set and recMainLo/recMainHi already made.
    // Values below are the ones the framework previously baked in, unchanged, so behaviour is identical.
    .hle =
        {
            // Two I/O / hardware-service windows, NEVER game logic. The guard on register_() keeps engine
            // FUN_xxxx out of the HLE table (those are owned top-down via the override registry):
            //   [0x8001C000,0x8001E000) the engine's CD/SPU I/O glue (libcd-wrapper readers, SPU-mix)
            //   [0x80080000,0x8009E000) the SCEI library text (libgpu/libetc/libcd/libgs/libmdec) plus the
            //                           kernel thread primitives at 0x80080xxx
            // Game/engine LOGIC lives at [0x8001E000,0x80082000) and in the overlays (0x8010xxxx+) — both
            // outside these windows, which is what makes the guard meaningful.
            .windowLo = {0x8001C000u, 0x80080000u},
            .windowHi = {0x8001E000u, 0x8009E000u},
            // Resident-code range for the guest-backtrace heuristic (physical; overlays sit above the main
            // text, so this is wider than recMainLo/recMainHi and must be stated explicitly).
            .codeScanLo = 0x00010000u,
            .codeScanHi = 0x00120000u,

            .decDctInSync = 0x8009CAECu,    // libmdec DecDCTinSync
            .decDctOutSync = 0x8009CB80u,   // libmdec DecDCToutSync
            .cdReadSync = 0x8008A96Cu,      // FUN_8008a96c(mode, result)
            .cdDataSync = 0x8008B4B8u,      // FUN_8008b4b8(mode)
            .cdInitHandshake = 0x8008B2D8u, // low-level CdInit controller-ready handshake
            .gpuTimeoutArm = 0x800834A0u,   // FUN_800834a0 arm deadline
            .gpuTimeoutCheck = 0x800834D4u, // FUN_800834d4 check (not timed out)
            .gpuTimeoutDeadlineVar = 0x800A5ADCu,
            .gpuTimeoutFlagVar = 0x800A5AE0u,
            .changeThread = 0x80080880u, // ChangeThread — the universal yield/task-end primitive
            // libgte SetGeomOffset / SetGeomScreen — the camera projection, recorded where the game STATES it
            // rather than read back out of the GTE at draw time (see proj_params.h). Both RE'd and 0-diff
            // (engine-ownership audit rows 40-41): OFX 160 / OFY 120, H 350 — with H re-stated per area by
            // Pool::finalViewInit at that area's own draw range.
            .setGeomOffset = 0x800846D0u,
            .setGeomScreen = 0x800846F0u,
            // VSync TRAP: correct FOR THIS PORT, whose PC-native frame loop owns all timing, so nothing may
            // reach libetc VSync in any mode. A port still running the guest's own loop on the substrate
            // would leave this zero and register a faithful VSync instead.
            .vsyncTrap = 0x80085900u,
        },

    // Transitional compatibility mirror only. Shipping picture ownership is answered directly by
    // TombaRuntime::guestVramIsPicture(); this field remains while legacy framework consumers still
    // require the flat GameConfig view and must not become a second policy authority.
    .preserveVramBackdrop = 0u,

    // Display fields one gpu_pace_frame call represents — the same fact as
    // tomba::FrameCadence::kRetailVblanksPerLogicFrame (game/core/frame_cadence.h), which now owns
    // the decision and publishes it into the guest's own quota byte at 0x1F800235. This stays a
    // static initializer because it feeds the framework's flat config view, so it cannot call a
    // per-Core owner; both values are read out of MAIN.EXE and must stay equal.
    // NOTE: if the engine legitimately VARIES that byte per frame (slowdown frames), a constant is
    // wrong and it needs a narrow typed cadence interface, not another legacy callback.
    .paceQuota = tomba::FrameCadence::kRetailVblanksPerLogicFrame,
    // crt0 stack-top bias, MEASURED by psxport tools/crt0_extract over this game's own boot
    // executable (MAIN.EXE, entry 0x800896E0). `declared = 1` is mandatory: crt0_plan REFUSES a boot when it is 0,
    // because 0 is a REAL measured answer for some crt0s and so cannot double as "unset".
    .stackBias = {1, -8},
    // The scheduler's guest task ENTRY PCs. These were literals inside psxport's own
    // `pc_scheduler.cpp` until 2026-08-13; the framework was testing THIS game's addresses directly,
    // which meant it scheduled Tomba! 2 specially, left those branches dead for every other port, and
    // gave a new port no way to declare its own. P1.7c had already moved the task BODIES to the
    // `schedStageBody` hook — this is the mapping that never moved with them.
    //
    // `nativeHandler` = the native per-frame stanzas own this entry (PcScheduler::hasNativeHandlerForEntry).
    // `hasFiberBody`  = a FRESH task at this entry starts a coro fiber running `fiberBody`.
    // The two are independent: the DEMO/GAME dispatchers have native handlers and no fiber body, the
    // preload bodies have fiber bodies and no native handler, and SOP area-load has both.
    .schedEntries = g_tomba_sched_entries,
    .schedEntryCount = (uint32_t)(sizeof(g_tomba_sched_entries) / sizeof(g_tomba_sched_entries[0])),

    // Synchronous FUN_80044BD4 ownership. These are appended framework fields so positional consumers
    // keep their ABI; this designated initializer keeps the measured Tomba layout explicit.
    .syncWaitDoneFlag = 0x1f80019bu,
    .syncWaitParam2 = 0x801fe0deu,
    .syncWaitParam3 = 0x801fe0ddu,
    .syncWaitTaskGp = 0x1f800000u,
    .syncWaitForceCloseRa = 0x80044c2cu,
    .syncWaitSpawnRa = 0x80044c50u,
    .syncWaitFinishRa = 0x80044c64u,
    // Tomba!2 is a 320x224 game: the cutscene letterbox rects the guest draws sit at rows 0..11 and
    // 212..223, i.e. flush to the bottom of a 224-line screen (RE note in game/render/cine_bars.cpp,
    // "sized for a 320x224 PSX display"). It never programs GP1(07) — 16,061 GP1 writes in one
    // measured session, none of them 05/07/08 — so without this the guest render paths presented the
    // framework default 240 and showed 16 rows below the bottom bar that no console scans out.
    .guestDisplayHeight = 224,

    // The two overlapping fade drivers the `debug fadewatch` state tap dumps (2026-07-01, "garbage
    // during fade"). They were hardcoded inside the framework's gpu_vk_present until 2026-09-19,
    // where they were read on every title that turned the channel on; they are Tomba! 2 facts and
    // belong here. bg_scene_transition_sm's struct base, and the scratchpad word holding
    // ov_sop_field_mode's outer state machine.
    .fadewatchDriverStruct = 0x80100400u,
    .fadewatchFieldModeSmPtr = 0x1f800138u,
    // B-key bug reports are saved under <user data>/tomba2/bug-reports/.
    .userDataName = "tomba2",
};

const GameConfig &tomba::title::measuredConfig() {
  return g_tomba_config;
}
