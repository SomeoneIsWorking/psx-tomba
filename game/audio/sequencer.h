// game/audio/sequencer.h — the libsnd sequencer: its per-VBlank tick wrapper, the sequence/channel
// scheduler that wrapper calls, and the channel leaves both dispatch to.
//
// The guest's structure: 0x800909C0 is a trampoline that runs the installed user callback (when the
// slot is non-null) and then unconditionally calls *SsSeqCalled (0x80090BD0). SsSeqCalled is a
// reentrancy-guarded double loop over sequences x channels which tests eight independent bits in
// each channel record's flags field (+152) and routes to the matching leaf.
//
// Addresses, field layouts, confidence and verification history live in docs/engine_re.md's libsnd
// sections; each method's own comment names its guest address and stack frame.
#pragma once

#include <cstdint>

class Core;

namespace tomba::audio {

// One instance per Core, embedded on Engine.
class Sequencer {
public:
  Core *core = nullptr;

  // 0x800909C0 — per-VBlank tick wrapper: the user callback, then the SsSeqCalled pointer.
  void frameTick();

  // 0x80090BD0 — the sequence/channel scheduler and its flags-field bit routing.
  void seqChannelDispatch();

  // Leaves selected by a flags-field bit. 0x800910F0, 0x80091050, 0x80091910, 0x80091970,
  // 0x80095A9C, 0x80095B90, 0x80094B50.
  void channelPitchSelectDispatch();
  void channelReleaseClear();
  void channelStopFlagSet();
  void channelNoteInit();
  void channelVolumeSnapshot();
  void channelKeyEventScan();
  void channelKeyRegisterMerge();

  // The per-tick ramp leaves. 0x80090E40 (portamento), 0x80092080 (ADSR).
  void channelPitchSlideTick();
  void channelEnvelopeRampTick();

  // The SPU voice-register leaves. 0x80095530, 0x800962B0, 0x80091810.
  void channelVoiceRegisterWrite();
  void channelVoiceSelectPrep();
  void channelVoiceKeyOn();

  // Tone-table and stream leaves. 0x80090160, 0x80092310, 0x80092420, 0x80094150, 0x80094474.
  void channelStreamAccumulate();
  void channelToneRecordCopy();
  void channelToneRecordCopyWide();
  void voiceAllocateOrSteal();
  void channelNotePeriodCompute();

  // 0x800931C0 — per-frame SPU voice-state flush, run twice a field.
  void voiceStateFlush();

  // Declares every native override this class owns, one call per leaf group.
  void registerOverrides();
};

// The per-group declaration entry points, each defined beside the leaves it declares.
void declareDispatchOverrides();
void declareChannelFlagOverrides();
void declareVoiceWriteOverrides();
void declareToneRecordOverrides();
void declareVoiceAllocOverrides();
void declareVoiceStateOverrides();

} // namespace tomba::audio