#include "cd/engine_cd_state.h"

#include "cd/engine_cd.h"
#include "blob_codec.h"

namespace tomba {

bool EngineCdState::save(psx::platform::BlobWriter &out, std::string &) const {
  const EngineCd::DeferredMusic &music = cd_.deferredMusic();
  out.boolean(music.pending);
  out.u8(music.channel);
  out.u32(music.start);
  out.u32(music.end);
  return true;
}

bool EngineCdState::load(psx::platform::BlobReader &in, std::string &error) {
  EngineCd::DeferredMusic music;
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
