// render_frame.cpp — Render::frame/frameX, the per-frame render orchestrators (guest bodies).
//
// RE (MAIN.EXE, tools/disas.py):
//   0x8003f9a8 render orchestrator: jal 0x8004fd30, 0x80025d98, 0x8003bf00, 0x8003eec0, 0x8003b588,
//     0x8003bb50, 0x8003bcf4, 0x8003d0bc(a0=0x800f2418), 0x8003f024, 0x8003df04, 0x8003c048.
//   0x8003fa44 (transition twin): jal 0x8004fd30, 0x80025d98, 0x8003bf00, 0x8003eec0, 0x8003b588,
//     0x8003bb50, 0x8003bcf4, 0x8003c048, 0x8003f024.

#include "cfg.h"
#include "core.h"
#include "guest_call.h"
#include "render.h" // class Render — methods live here
#include <stdio.h>

// 0x8003f9a8 — per-frame render orchestrator; runs as the guest body.
void Render::frame() {
  Core *c = mCore;
  if (cfg_dbg("rfprobe")) {
    static int n = 0;
    if ((n++ % 60) == 0) {
      cfg_logf("rfprobe", "ov_render_frame run #%d", n);
    }
  }
  psx::cpu::callGuestNow(*c, __func__, 0x8003f9a8u);
}

// 0x8003fa44 — mid-transition render orchestrator twin (reduced pass set). Same rule: substrate always.
void Render::frameX() {
  Core *c = mCore;
  psx::cpu::callGuestNow(*c, __func__, 0x8003fa44u);
}
