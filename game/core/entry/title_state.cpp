#include "core/entry/title_state.h"

#include "blob_codec.h"
#include "cd/engine_cd.h"
#include "core/frame/draw_replay.h"

namespace tomba {

bool TitleState::save(psx::platform::BlobWriter &out, std::string &) const {
  const EngineCd::DeferredMusic &music = cd_.deferredMusic();
  out.boolean(music.pending);
  out.u8(music.channel);
  out.u32(music.start);
  out.u32(music.end);
  return true;
}

bool TitleState::load(psx::platform::BlobReader &in, std::string &error) {
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

void TitleState::restored(Core &) {
  replay_.forget();
}

} // namespace tomba
