# Tomba! 2 native/dynarec RE frontier

This is the ordered execution-evidence chain for the active title. It preserves verified behavioral
and address facts while replacing the removed offline-generated product with runtime Lightrec.
Tomba! 1 has its own frontier under `titles/tomba1/docs/re-frontier.md` and remains deferred until
Tomba! 2 representative gameplay is complete.

## Execution spine

### T2-00 — Preserve authenticated title and overlay identity
- status: re-partial
- deps:
- evidence: Existing binary/run evidence identifies `SCUS_944.54` and the recorded resident/overlay behaviors. C044 records an independently checked MAIN.EXE digest; address-collision evidence identifies `0x801113B4` in A03 and A0B with different entry shapes. The launcher provisioner now owns a size/SHA-256 manifest; its original-disc qualification and remaining runtime boundary are recorded in issue 0005.
- where: title provisioning metadata; binary-backed findings; user-supplied disc outside Git
- gap: Enforce exact-content identity in product loading; historical binary evidence and runtime generation tokens are not an authenticated input manifest. See issue 0005.

### T2-01 — Execute authenticated images through psxport Lightrec
- status: re-partial
- deps: T2-00, T2-06
- evidence: The shipping Lightrec product completes a real-image native startup frame with nonzero translated execution and zero fallback; exact counters and the reached second-frame failure are recorded in project-state S001 and issue 0007. Linked products pass the execution-boundary discriminator.
- where: shared `external/psxport/` executor; title composition in `cmake/tomba2_port.cmake`
- gap: Complete authenticated overlay activation and representative gameplay, with measured invalidation and bounded fallback accounting; startup is not conformance. Diagnostic interpreter mode is not a product selector.

### T2-02 — Prove a resident native override and original call
- status: in-progress
- deps: T2-01
- evidence: Production catalog tests exercise resident generation binding, scoped original calls, and a different-image collision negative through Lightrec. An isolated authenticated MAIN.EXE probe now exercises the shipping `Str::length` native owner, its scoped original guest body, and wrong-address/disabled negatives with 9 translated blocks and zero fallback; issue 0005 owns the full result.
- where: title-native owner in `game/`; shared image-aware override/original-call dispatcher in `external/psxport/`
- gap: Reach this native owner on a real game route and compare the resident original call there; the isolated real-byte fixture does not prove retail reach.

### T2-03 — Prove colliding-overlay override and original calls
- status: in-progress
- deps: T2-02
- evidence: Binary evidence establishes that numeric address `0x801113B4` belongs to different entry shapes in A03 and A0B; address-only dispatch is therefore invalid. The title activates loaded A00–A0L MODE images through both area-loader paths. A local diagnostic with manifest-authenticated A03/A0B bytes proves two bounded Lightrec original returns, image-scoped fixture selection, retirement, and a wrong-image guest negative (14 translated blocks, zero fallback); issue 0005 records the exact branches and limits.
- where: overlay-authentication owner and shared runtime dispatcher; title-owned override selected by complete image identity
- gap: No production A03/A0B native owner is declared and no real game route has reached this collision under both overlays. Runtime overlay-content authentication, reached owner/original-call comparison, and representative gameplay remain open; the diagnostic fixture does not establish title conformance.

### T2-04 — Re-establish the current boot-to-free-roam frontier
- status: todo
- deps: T2-03
- evidence: Recorded pre-migration runs reach GAME at frame 25 and free-roam at frame 216, complete 620/620 presentation fences, and preserve the fatal guest-VSync boundary at `0x80085900`.
- where: native title runtime and frame driver; shared Lightrec executor and PSX services
- gap: Reach the same frontier with all native owners active, nonzero Lightrec execution, relevant overlay invalidation, and no generated guest code or interpreter in the gameplay link/selector surfaces.

### T2-05 — Prove representative interactive gameplay
- status: todo
- deps: T2-04
- evidence: Existing boot and bounded free-roam captures are checkpoints, not representative gameplay conformance.
- where: shipping product plus independent emulator/hardware or separately built test oracle; deterministic gameplay scenario
- gap: Cover real player input, guest PC/registers and memory, interrupts/timing, relevant CD/GPU/SPU behavior, native override reachability, and declared frame-time/correctness budgets with positive and negative controls.

### T2-06 — Remove the Tomba! 2 static path
- status: re-verified
- deps: T2-00
- evidence: The generated tree, generator entry point, emission-only seeds, static registry and build inputs, and generated-symbol tests are absent. Provisioning retains authenticated executable and overlay bytes only.
- where: absent by design; enforced by title CMake and provisioning boundaries
- gap: None for removal. Fresh product build/launch evidence belongs to T2-01 through T2-05 and cannot fall back to the retired path.

## Area machine facts

- `0x80108F60[area]` selects the sm[0x4c] handler after a load: 2 field run, 4/5/6 the GAME-image handlers
  `0x80107230` / `0x8010766C` / `0x80107790` (areas 2, 3, 7, 20). Their first state runs `FUN_8007B18C` and the
  area init chain and needs 695,994 cycles (2 display fields); `Engine::submode1` resumes them.
- `0x800BF89C`: 2 during the scripted opening, 4 in ordinary play. In mode 2 sm[0x4e] = 9 and Start skips to area 0
  (`fieldRun` cases 9, 10, 7, 8, 6).
- `0x80100400`: the 8-slot object array (stride 0x4C); `FUN_800263E8` fills it from the per-area type list at
  `0x8009D414[area]`, `FUN_80026368` dispatches slot type through the handler table at `0x8009D314`. Types 2..35 point
  into whichever MODE image owns the area, so a slot seeded for another area's code dispatches into the wrong image.
- Tomba! 2 has no retail debug menu or warp in the 28 disc images or the MAIN.EXE strings (searched for debug,
  warp, select, stage, test, cheat; stage ids reach only START, DEMO, GAME, via `FUN_80052078`). The retail area travel is Magic Wings (item
  text "fly to anywhere Tomba has been"), a player feature. A community debug menu exists outside the disc: the Tomba
  Club "Debug Menu (Press L3 to toggle)" GameShark/DuckStation code list for SCUS-94454 by unicorngoulash, carried
  verbatim in `mstan/Tomba2Recomp` (`mods/sources/tomba2_debug_menu.cht`). It is MIPS written to `0x8000C000` with five
  call sites in the gameplay overlay re-pointed at it; its WARP TOOLS page launches an area/entry transition (issue 0034).

## Render producers

### T2-R1 — Key the largest unkeyed draw submitters
- status: re-partial
- deps: T2-03
- evidence: Decompiled with `decomp_pipeline.py` against SOP and A08 at the MODE slot load base 0x80108F9C, body present on every target. SOP FUN_8010C26C is FUN_80115598's tile grid with no V bias plus three CLUT palette cycles (scripts at 0x8010D2FC stride 12, countdowns at 0x8010D390, `{0xFF, n}` rewinds, LoadImage of a 16x1 row). SOP FUN_80109FE0 loads the scene camera from 0x1F8000F8 into GTE control 0-7 and draws each visible block (`list+0xC` blocks, `list+0x10` u16 indices, count at `list+6`) through the SOP GT3/GT4 list emitters 0x801099B4/0x80109C80; FUN_8010A0E0/0x8010A3AC rebuild that list each frame. FUN_8010C79C is the narration crawl: line strings from table 0x8010D320 through drawText and FUN_80078CA8. A08 FUN_80116904 draws 32 rain streaks from a fixed-seed LCG lattice (seed node+0x50, multiplier 0x801450D8) around the camera's 2048-unit cell, one LINE_G2 + DR_TPAGE per drop, trail per drop at 0x801485E8+4i. Ports pass recordcheck mismatched=0 at 4:3 30 fps and give byte-identical RAM, scratchpad and shots to the guest bodies at intro f300/f600/f990 and area-8 f540/f548.
- where: `game/render/tile_grid_layer.cpp`, `sop_ground.cpp`, `rain_streaks.cpp`, `model_packet.cpp`, `game/ui/font.cpp`; `tests/test_producers.cpp`
- gap: SOP 0x8010BF54 and A08 0x80117628/0x80117E84 draw whole models through FUN_8011731C and 0x80027768, which name no primitive elements yet.

### T2-R2 — Model list emitters with a canvas-wide screen cull
- status: re-partial
- deps: T2-R1
- evidence: Disassembled from the authenticated overlays at 0x80108F9C. A05 0x8013544C/0x8013590C, A07 0x8012CDF4/0x8012D2B4 and A08 0x80129BAC/0x8012A06C are one lit GT3/GT4 body (only local jump targets differ): RTPT/RTPS, FLAG reject, NCLIP, depth by colour-word bits 24-25 (0 AVSZ, 2 nearest, 1/3 farthest SZ, then >> 2), per-corner intensity 2 x |corner - light| (light s16 xyz at 0x1F800160, cap 0x4000) or a3 when nonzero, one DPCS per corner. A08 0x80140FBC/0x801411D8 are A00 0x801465EC/0x801467BC plus a UV scroll (flag bit 2, s16 at 0x80145A6E; the GT3 copy adds it to uv0 twice and never to uv2); the plain GT4 has no screen cull. A01 0x801316A8/0x80131BB0 is that lit body plus flag bit 6 (hide while 0x1F80009C is set; GT3 tests it after staging the depths, GT4 before) and bit 3 (bucket + 0x80, clamped to 0x7FF). Resident 0x8007FDB0/0x8008007C and SOP 0x801099B4/0x80109C80 are one unlit body differing by data: stage 0x1F800080/84 vs 0x1F800000/04, colour mask 0xFFF0F0F0 vs 0x00F0F0F0 (GT4 word +4 0xFFF0F0F0), depth mode flag & 3 vs the whole byte (1 farthest, 2 nearest, else AVSZ), farthest staged at sp+0 and nearest at sp+0xC (GT3) / sp+0x10 (GT4), and the resident GT4 runs RTPS on corner 3 before RTPT. A0B 0x80112A24/0x80112CF0 and A0C 0x80113788/0x80113A54 are SOP's body. A01 0x80130838 (frame 0x38): after the bucket, bit 3 sways each corner, SX += or -= rcos(phase + VX) >> 9 (+ when VZ is odd), SY += rsin(phase + VZ) >> 9, phase s16 at 0x80139004, then bucket + 0x80; else bit 4 adds the bytes at 0x801388EC/0x801388EE to the UVs. Its GT4 0x80130D9C never sways. A01 0x8012F8D8/0x8013000C: bit 3 sways (bucket + 0x96), else bit 6 hides, else DPCS per corner with IR0 = SZ / 4, plus the top 10 bits of the LCG at 0x1F800080 (x 0x41C64E6D + 0x3039) when bit 2 is set; GT4 bit 7 cues only corners whose bits 3..6 are set and skips the UV scroll. A01 0x80132690/0x801329C4 (the drawer 0x80132DC0's branch with neither 0x800B8873 nor 0x800B8816) is SOP's unlit body keeping every code byte, staging SZ at its 16-byte frame for every record and depth-cueing each corner by SZ / 4. The other overlays' copies, normalised over j/jal targets and branch offsets: A07 0x801311D0/0x801313A0 and A0L 0x80112DEC/0x80112FBC are A00's plain pair. A02 0x801246A4/0x8012496C and A07 0x8012C7E0/0x8012CAA8 are A00's ground pair picking the farthest SZ for flag mode 2 too; A0L 0x8010AA4C/0x8010AD40 does as well and adds 0x64 to GT3 mode 1 and GT4 mode 2. A0A-A0F and A0H-A0J share one unlit body (0x801103F4/0x80110698 in A0A): frame 0x10, flag byte 0 AVSZ, 1 farthest, anything else nearest, SZ staged at sp+0 only for those two; its GT4 jumps past the link after sorting a farthest/nearest record, so only averaged quads draw. A0G 0x8010BC40/0x8010BF28, A0I 0x8010B3DC/0x8010B6C4, A0H 0x8010AF58/0x8010B240 and A0J 0x8010A3AC/0x8010A69C are SOP's body with a near clamp before compression: depth += lift (A0J 0x50), and (u32)(depth - 0x28 + reach) < reach sets 0x28 (reach 0x1B7, A0H 0x27F); SZ and OTZ are never negative, so only A0G-A0I's [0, 0x28) band is reachable. A06 0x8013BA44/0x8013BD40 is SOP's body with the lit depth code and flag bit 2 hiding the record while 0x1F80009C is set; A06 0x8013C0D8/0x8013C5B4 is the lit body with that hide; A06 0x8013CF00/0x8013D1E4 (drawer 0x8013D568) is A01's third pair on the resident stage with flagged depth from the low 2 bits, the resident GT4 corner order and, for flag bit 3, (u16 0x8014A450 >> 4) added to each corner's U after the last UV. A0A 0x801142EC, A0B 0x801141B0, A0D 0x80116B9C and A0F 0x80116778 are A00's tile grid 0x80115598 without the V bias (A0B's starry backdrop). Natives are byte-identical to all 70 retail bodies on 14000 random lists (`tests/test_authentic_model_emitters.cpp`), a quarter of the cases with a short focal length so the near clamp is reached.
- where: `game/render/model_packet.cpp`, `lit_model_emitter.cpp`, `unlit_model_emitter.cpp`, `sway_model_emitter.cpp`, `overlay_gt3gt4.cpp`, `overlay_ground_gt3gt4.cpp`, `tile_grid_layer.cpp`
- gap: bodies that differ from every native are not ported: A02 0x80124DB8/0x80125100, A03 0x80110094/0x80110290, A04 0x801175FC/0x801178A0 and 0x8013D0F8/0x8013D568, A05 0x8013484C/0x80134B74 and 0x8013AC90/0x8013AF0C, A07 0x8012DBB4/0x8012DE60, A0A 0x8010F97C/0x8010FD74, A0D 0x80112DE4/0x801131B0, A0E 0x80113AEC/0x80113DE0 (A06's 0x8013BA44 plus a world-X cut while 0x1F80009C is set, mid-projection), A0F 0x80114A5C/0x80114E70, A0G 0x8010C3A4/0x8010C664, A0K 0x80114F1C/0x8011562C and 0x801163E0/0x80116708.

### T2-R3 — Render-record reuse
- status: re-verified
- deps: T2-R1
- evidence: FUN_8007AAE8 is the only allocator of render records: it pops a LIFO stack (count s16 0x800ED098, cursor 0x800E7E74 walking record pointers) and leaves a0 = 0x800E0000, a1 = the count, a2 = 0x800F0000 and, on success, v1 = the cursor. Records are pushed back by FUN_8007ADDC (despawn class 4, inlined in `Spawn::despawn`), 0x80058230 and inline copies in A00 (3), A04 (2), A05, A07 (2), A0B and OPN; the pool is initialised at 0x8007B140/0x8007B1E4. A despawn followed by a spawn therefore reuses the record just freed. Measured at the area 0 load: 18 records popped at f227, freed and popped by other owners at f228; no drawn record was seen reused inside a scene in about 3000 frames of areas 0, 1, 2 and 8.
- where: `game/world/render_record_pool.cpp`, `graphics_bind.cpp`; `tests/test_render_record_pool.cpp`
- gap: none for the allocator; no in-scene respawn of a drawn record has been captured.
