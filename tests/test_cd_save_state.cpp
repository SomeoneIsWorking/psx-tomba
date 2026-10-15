// A save state carries the area music EngineCd holds back for a dialog, through the runtime's nativeState port.
#include "cd/engine_cd.h"
#include "core/entry/runtime.h"
#include "core/entry/session.h"
#include "game.h"
#include "machine_state.h"
#include "state_blob.h"
#include "testutil.h"

#include <memory>
#include <string>

namespace {

void test_deferred_area_music_survives_a_save_state() {
  tomba::Runtime runtime;
  auto saved = std::make_unique<Game>(runtime, psx::config::Config{});
  tomba::sessionOf(&saved->core).cd.deferredMusic() = {.pending = true, .channel = 4, .start = 84515, .end = 97979};
  psx::state::NativeStatePort *savePort = runtime.nativeState(saved->core);
  CHECK(savePort != nullptr);
  psx::state::BlobWriter out;
  std::string error;
  CHECK(savePort->save(out, error));

  auto restored = std::make_unique<Game>(runtime, psx::config::Config{});
  CHECK(!tomba::sessionOf(&restored->core).cd.deferredMusic().pending);
  psx::state::NativeStatePort *loadPort = runtime.nativeState(restored->core);
  CHECK(loadPort != nullptr && loadPort->version() == savePort->version());
  psx::state::BlobReader in(out.bytesOut());
  CHECK(loadPort->load(in, error));
  const tomba::EngineCd::DeferredMusic &music = tomba::sessionOf(&restored->core).cd.deferredMusic();
  CHECK(music.pending);
  CHECK_EQ(music.channel, 4u);
  CHECK_EQ(music.start, 84515u);
  CHECK_EQ(music.end, 97979u);
}

void test_truncated_deferred_music_is_refused() {
  tomba::Runtime runtime;
  auto game = std::make_unique<Game>(runtime, psx::config::Config{});
  psx::state::NativeStatePort *port = runtime.nativeState(game->core);
  CHECK(port != nullptr);
  const std::uint8_t pendingOnly[] = {1u};
  psx::state::BlobReader in(pendingOnly);
  std::string error;
  CHECK(!port->load(in, error));
  CHECK(!tomba::sessionOf(&game->core).cd.deferredMusic().pending);
}

} // namespace

int main() {
  RUN(deferred_area_music_survives_a_save_state);
  RUN(truncated_deferred_music_is_refused);
  return psx::test::summary();
}
