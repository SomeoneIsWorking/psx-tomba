#include "cd/tomba_cd_state.h"

#include "cd/tomba_cd.h"
#include "state_blob.h"

namespace tomba {

bool TombaCdState::save(psx::state::BlobWriter &out, std::string &) const {
  const TombaCd::DeferredMusic &music = cd_.deferredMusic();
  out.boolean(music.pending);
  out.u8(music.channel);
  out.u32(music.start);
  out.u32(music.end);
  return true;
}

bool TombaCdState::load(psx::state::BlobReader &in, std::string &error) {
  TombaCd::DeferredMusic music;
  music.pending = in.boolean();
  music.channel = in.u8();
  music.start = in.u32();
  music.end = in.u32();
  if (!in.ok()) {
    error = "tomba2.cd: the deferred area music is truncated";
    return false;
  }
  cd_.deferredMusic() = music;
  return true;
}

} // namespace tomba
