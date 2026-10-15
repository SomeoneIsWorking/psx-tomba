#include "stream_field_turn.h"

#include "core.h"
#include "game.h"
#include "stock_libcd.h"

#include <lucent/log.h>

namespace tomba1 {
namespace {

void advanceStreamAudio(Core &core) {
  core.game->spu_audio.frame();
}

void pumpStreamDisc(Core &core) {
  core.game->cd.stockLibrary()->pumpStream(core, CD_STREAM_MAX_BURST);
}

constexpr StreamFieldTurn kStreamFieldTurn{.advanceAudio = advanceStreamAudio, .pumpDisc = pumpStreamDisc};

void logStreamState(Core &core, const char *phase) {
  const Game &game = *core.game;
  const psx::cd::CdController &controller = game.cdc;
  const psx::audio::XaStream &xa = game.xa;
  const std::uint64_t now = controller.tick_now ? controller.tick_now(controller.tick_context) : 0u;
  lucent::debug("tomba1-stream-field",
                "{} delivered={} now={} deadline={} armed={} read={} first={} following={} bfrd={} "
                "fifo={}/{} irq={} queue={}->{} lba={} xa(active/push)={}/{} xa(wr/rd/pulls/sectors)="
                "{}/{}/{}/{} stream-dma(inflight/owed/dicr/pending)={}/{}/0x{:08X}/0x{:X}",
                phase,
                game.cd.stream_delivered,
                now,
                controller.drive_deadline_ticks,
                controller.drive_event_armed,
                controller.reading,
                controller.first_sector_pending,
                controller.following_sector_ready,
                controller.bfrd,
                controller.data_rd,
                controller.data_n,
                controller.currentIrqType(),
                controller.q_head,
                controller.q_tail,
                controller.loc_lba,
                xa.active,
                xa.push_mode,
                xa.wr,
                static_cast<std::uint32_t>(xa.rd),
                xa.pulls,
                xa.sectors,
                core.mem_r32(0x8001CA08u),
                psx::dma::doneOwed(core, 3),
                core.mem_r32(0x1F8010F4u),
                core.pending_work);
}

} // namespace

void StreamFieldTurn::service(Core &core) const {
  advanceAudio(core);
  pumpDisc(core);
}

void serviceStreamField(Core &core) {
  logStreamState(core, "before");
  kStreamFieldTurn.service(core);
  logStreamState(core, "after");
}

} // namespace tomba1
