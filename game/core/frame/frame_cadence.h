// game/core/frame_cadence.h — class FrameCadence, the ONE owner of Tomba! 2's frame-rate decision.
//
// WHAT THE GUEST'S OWN LEVER IS. Recovered from the authenticated resident image
// (SCUS_944.54 → MAIN.EXE, text [0x80010000,0x800AE800), Ghidra headless; the instruction
// words below are the image's own). FUN_80050b08 — the StrPlayer main loop crt0 reaches by
// `jal` at 0x800896E0 — paces every displayed frame through a vblank gate INSIDE its loop body:
//
//   80050C8C  A6C0809C  sh    zero,-0x7f64(s6)   ; DAT_800e809c  = 0   (s6 = 0x800F0000, u16)
//   80050CB0  0C01E22B  jal   0x800788AC        ; per-frame state update
//   80050CB8  0C014798  jal   0x80051E60        ; task scheduler -> builds the OT
//   80050CC0  0C0203DB  jal   0x80080F6C        ; DrawSync(0)
//   80050CC8  3C031F80  lui   v1,0x1f80
//   80050CCC  96C2809C  lhu   v0,-0x7f64(s6)    ; v0 = DAT_800e809c
//   80050CD0  90630235  lbu   v1,0x235(v1)      ; v1 = DAT_1f800235
//   80050CD8  0043102B  sltu  v0,v0,v1         ; v0 = (dwell counter < quota)
//   80050CDC  10400006  beq   v0,zero,0x80050CF8  -> release the gate
//   80050CE0  3C04800F  lui   a0,0x800f
//   80050CE4  9482809C  lhu   v0,-0x7f64(a0)    ; spin: reload the counter
//   80050CEC  0043102B  sltu  v0,v0,v1
//   80050CF0  1440FFFC  bne   v0,zero,0x80050CE4  -> keep spinning
//   80050CF8  0C0141B4  jal   0x800506D0
//   80050D08  1072FFDC  beq   v1,s2,0x80050C7C   -> next pass
//
// The counter's ONLY incremental writer is libapi's VSyncCallback slot 4, installed by the
// loop's own init at 0x80050BA8 (`jal 0x80085BB0`, a0 = 0x800506B4). FUN_80085BB0 forwards
// (4, cb) to the libapi service slot at 0x80085BC4, which FUN_80085CB4 filled with the return
// value of FUN_80086230 — 0x800862F4, the registrar whose body is
// `(&DAT_800abdc0)[slot] = cb`. The registered body is a pure incrementer:
//
//   800506B4  3C03800F  lui   v1,0x800f
//   800506B8  6C628064  lhu   v0,-0x7f64(v1)    ; v0 = DAT_800e809c
//   800506C0  24420001  addiu v0,v0,1
//   800506C4  5C628064  sh    v0,-0x7f64(v1)    ; DAT_800e809c = v0 + 1
//   800506C8  03200008  jr    ra
//
// It runs once per vblank because it sits in the 8-slot vsync-callback table at 0x800ABDC0,
// which the vblank interrupt handler FUN_80086288 walks (game/core/libapi_intr.cpp owns that
// handler natively).
//
// THERE IS A SECOND WRITER FAMILY, OUTSIDE THE RESIDENT IMAGE. The six sites above are every
// reference in MAIN.EXE, and they are not every reference in the game. Scanning the 28 provisioned
// overlay images for the same address-forming idiom finds it in two of them, at byte-identical
// looking code (file offsets and load bases not established), reported here as what they are:
//
//   A0L.BIN +0x0099AC  0xA062019C  sb    v0,0x19C(v1)   ; DAT_1f80019C = 2  (StrPlayer "swap")
//   A0L.BIN +0x0099B8  0xA462809C  sh    v0,-0x7f64(v1) ; DAT_800e809c = 1
//   DEMO.BIN +0x001040 0xA062019C  sb    v0,0x19C(v1)
//   DEMO.BIN +0x00104C 0xA462809C  sh    v0,-0x7f64(v1)
//
// Each is preceded by `lui v1,0x800F` (0x3C03800F) and `li v0,1` (0x24020001). So the guest does
// have a per-frame cadence lever — it is just not the quota byte. It SHORTENS AN INDIVIDUAL FRAME
// by writing the dwell counter to 1, which releases FUN_80050b08's gate after one vblank instead of
// two, while setting the swap-mode byte so the loop takes its 0x80050D00 `DAT_1f80019c == 2` arm
// (PutDispEnv, mode back to 1, parity flip). It never changes the rate; it changes one frame's
// duration. Whether either body is REACHED in this product is not measured, and the header states
// that rather than the other way round. This is why consumeVblank() below only READS the counter:
// a guest write mid-frame is a real value the guest chose, and overwriting it would erase the one
// piece of guest-authored cadence the game still has.
//
// WHAT THE GATE IS, AND IS NOT. One full pass of per-frame work runs per gate release, and
// NOTHING in that pass reads the quota: the pass is
// counter-reset -> FUN_800788AC -> FUN_80051E60 -> DrawSync(0) -> gate -> FUN_800506D0 ->
// PutDispEnv/PutDrawEnv/DrawOTag -> flip parity. The quota therefore does not scale, weight or
// gate any work; it is a pure RATE LIMITER. One logic frame per N vblanks is 60/N Hz of logic
// on a 60 Hz vblank, so the guest's own 60 fps mode is this byte equal to 1. That is why the
// port owns the byte rather than reimplementing a cadence around the loop.
//
// THE QUOTA IS A BOOT-TIME CONSTANT. A scan of every reference in the resident text
// finds exactly TWO sites for 0x1F800235 in the whole image:
//
//   80050A1C  24020002  li    v0,0x2           ; addiu v0,zero,2
//   80050A20  A0620235  sb    v0,0x235(v1)      ; the ONLY store, inside FUN_80050a0c
//   80050CD0  90630235  lbu   v1,0x235(v1)      ; the ONLY load, the gate's threshold
//
// The guest has NO runtime 30/60 switch: the literal `2` is compiled into the instruction word.
// The port is therefore the only thing that can decide, and this class is where it decides.
//
// WHY THIS IS NOT A `declareOverride`. 0x80050CC8..0x80050CF4 is a LABEL inside FUN_80050b08's
// body, reached by falling through, never by `jal`/`jalr` — the framework's image-scoped native
// contract (psxport/AGENTS.md, "An override key names a function entry that guest code reaches
// by jal/jalr") makes it ineligible as an override key, and the enclosing FUN_80050B08 is
// itself never dispatched by this product: psxport's native_boot calls GameRuntime::bootInit,
// which runs FUN_80050b08's INIT PREFIX and then hands iteration to TombaFrameDriver. So the
// gate's instructions do not execute here, and declaring an override for them would declare
// something with no reachability. What the port can own, and does, is the gate's STATE: the two
// guest fields the comparison reads, advanced through the guest's own mechanism.
#pragma once

#include <cstdint>

class Core;

namespace tomba {

// One concept: how many display fields one Tomba! 2 logic frame spans, and the gate's own
// comparison against the guest's dwell counter. It OWNS the decision and it REPORTS on the guest's
// halfword; it does not write that halfword except for the per-frame reset the guest itself makes.
// Composed into the frame transaction by TombaFrameDriver and consulted by Engine's per-field work,
// so the decision has exactly one home.
class FrameCadence {
public:
  // Back-pointer, wired once where every other Engine-owned subsystem is wired
  // (game/core/game_ctx.cpp createTombaContext), so no call has to thread a Core through.
  Core *core = nullptr;

  // ---- measured guest facts (see the banner; each is read off MAIN.EXE) -------------------
  // The quota byte: FUN_80050b08's gate threshold, stored by `sb v0,0x235(v1)`.
  static constexpr std::uint32_t kQuotaAddress = 0x1F800235u;
  // The dwell counter: a u16, stored by `sh`/`lhu` at offset -0x7f64 from a 0x800F0000 base.
  static constexpr std::uint32_t kDwellCounterAddress = 0x800E809Cu;
  // The value MAIN.EXE compiles in: the immediate of `li v0,0x2` (word 0x24020002) that the
  // guest stores once at boot. 2 fields per logic frame = the engine's 30 fps logic rate.
  static constexpr std::uint8_t kRetailVblanksPerLogicFrame = 2u;

  // ---- the decision ----------------------------------------------------------------------
  // Fields one logic frame spans. The port owns this, and publishes it into the guest's own
  // quota byte so every consumer — guest or native — reads one number from one place.
  [[nodiscard]] std::uint8_t vblanksPerLogicFrame() const {
    return vblanksPerLogicFrame_;
  }
  // Write the decision into the guest's quota byte, exactly as FUN_80050a0c stores its literal:
  // one byte at kQuotaAddress, nothing else. Called once at boot and again whenever the decision
  // changes, so the field can never disagree with the owner.
  void publish();

  // ---- the guest's own dwell counter -----------------------------------------------------
  // THE PORT WRITES THIS FIELD EXACTLY ONCE PER LOGIC FRAME, and that write is the guest's own:
  // FUN_80050b08 zeroes the counter at the top of every pass (0x80050C8C). Nothing in this class
  // increments it. The guest's incrementer is LAB_800506B4 in vsync-callback slot 4, and in this
  // product it genuinely runs — the port parks LibapiIntr::runVblankCallbacks at 0x80086288 into
  // libsnd's user-callback slot DAT_800AC430 (game/audio/sequencer.h), the per-field sequencer
  // tick at 0x800909C0 calls whatever is in that slot, and libsnd's SsSetTickMode (0x80090750,
  // which calls FUN_80085BB0 at 0x800908E4) is what installed the game's callback into slot 4.
  // So the counter is the GUEST's field, advanced by the GUEST's body, and the port observes it.
  //
  // An earlier revision of this class incremented the counter itself once per field. That is
  // wrong, and it was caught by reading WHO registers slot 4 rather than by reasoning about it:
  // it would have counted every field twice — once from the port, once from the guest's own
  // callback — and left the counter at 4 for a 2-field frame. A port that manufactures the
  // number it is checking cannot lie about anything, including about itself.
  void beginLogicFrame();
  // One display field the port SPENDS in this logic frame. Records the field; does NOT write the
  // guest's counter. Returns the counter the guest's own incrementer has left so far.
  std::uint16_t consumeVblank();
  // Close the frame's transaction. `frame` is the host frame number, for the report. The guest's
  // gate releases when its counter reaches the quota, so this states whether the guest's own
  // mechanism got there — and, separately, whether the port spent a field the guest's counter
  // never showed. Those are different failures and only one of them is the port's.
  void endLogicFrame(std::uint32_t frame);
  // The gate's own exit test, the `sltu` at 0x80050CD8: true once the counter has reached the
  // quota. Reported so the product can state whether the guest's gate would release this frame
  // rather than leaving the reader to trust the pacing.
  [[nodiscard]] bool gateOpen() const;
  [[nodiscard]] std::uint16_t dwellCounter() const;

  // ---- denominators ----------------------------------------------------------------------
  [[nodiscard]] std::uint32_t logicFrames() const {
    return logicFrames_;
  }
  [[nodiscard]] std::uint32_t vblanksSpent() const {
    return vblanksSpent_;
  }
  // Display fields this logic frame spent, and how many were spent OUTSIDE the per-field work. The
  // second must stay 0 for every frame: a non-zero value means a logic frame claimed display time
  // no scheduled work happened on.
  [[nodiscard]] std::uint32_t vblanksThisLogicFrame() const {
    return vblanksSpent_ - frameVblankBase_;
  }
  [[nodiscard]] std::uint32_t unaccountedVblanks() const {
    return vblanksSpent_ - accountedVblanks_;
  }
  // Called once the per-field work for every field advanced so far has been done.
  void accountVblankWork() {
    accountedVblanks_ = vblanksSpent_;
  }

private:
  [[nodiscard]] Core &host() const;

  std::uint8_t vblanksPerLogicFrame_ = kRetailVblanksPerLogicFrame;
  std::uint32_t logicFrames_ = 0;
  std::uint32_t vblanksSpent_ = 0;
  std::uint32_t frameVblankBase_ = 0;
  std::uint32_t accountedVblanks_ = 0;
};

} // namespace tomba
