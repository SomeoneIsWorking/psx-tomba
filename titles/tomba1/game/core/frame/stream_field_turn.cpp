#include "stream_field_turn.h"

#include "cd.h"
#include "core.h"
#include "dma_irq.h"
#include "field_rate.h"
#include "game.h"
#include "host_turn.h"

#include <lucent/log.h>

namespace tomba1 {
namespace {

void advanceStreamAudio(Core &core) {
  core.game->spu_audio.frameLogic();
}

void pumpStreamDisc(Core &core) {
  core.game->cd.pumpStream(&core, CD_STREAM_MAX_BURST);
}

constexpr StreamFieldTurn kStreamFieldTurn{advanceStreamAudio, pumpStreamDisc};

void logStreamState(Core &core, const char *phase) {
  const Game &game = *core.game;
  const CdcState &controller = game.cdc;
  const XaState &xa = game.xa;
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
                cdc_current_irq_type(&controller),
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
                dma_done_owed(core, 3),
                core.mem_r32(0x1F8010F4u),
                core.pending_work);
}

} // namespace

void StreamFieldTurn::service(Core &core) const {
  advanceAudio_(core);
  pumpDisc_(core);
}

void serviceStreamHostTurn(Core *core) {
  logStreamState(*core, "before");
  kStreamFieldTurn.service(*core);
  logStreamState(*core, "after");
}

void registerStreamFieldTurn(Core &core) {
  psx::cpu::registerHostTurn(core, serviceStreamHostTurn, psx::frame::FIELD_RATE_NTSC_MILLIHZ);
}

} // namespace tomba1
