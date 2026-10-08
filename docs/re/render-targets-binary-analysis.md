# Binary analysis: the remaining unowned type-0x20 render targets

> The display-time renderer (`Render::*Render`/`*Emit` host producers, `RenderQueue`, `pc_render`) named below was deleted; the picture is the guest's GP0 output on the GPU device. The guest RE stands as the reference for future producers.

Produced 2026-07-28 from the authenticated executable and overlays, with independent adversarial
verification of each recovered behavior. READ-ONLY: no agent edited the tree.

**Every spec below was re-derived by an independent verifier.** Five came back CORRECTED —
the corrections are inlined and the corrected algorithm replaces the original where one was
given. Do not port from an uncorrected memory of these functions.


## Summary

| target | area | ov | portable | verdict | errors found | effect |
|---|---|---|---|---|---|---|
| `0x8010C1D8` | 21 | a0l | straightforward | CORRECTED | 1F/1M/5m | A single node drawing two co-located layers in area 21 (the area Tomba rides a bird throug |
| `0x8013B118` | 4 | a04 | moderate | CORRECTED | 0F/2M/9m | An AREA-4 AMBIENT/ATMOSPHERE effect whose whole content switches on story progress |
| `0x80116904` | 8 | a08 | moderate | CORRECTED | 0F/1M/8m |  |
| `0x80110CA4` | 14 | a0e | moderate | CORRECTED | 0F/1M/4m | A large animated textured BACKDROP PLANE with a mirrored reflection and a glow band along  |
| `0x801110BC` | 11 | a0b | straightforward | CORRECTED | 0F/1M/5m | A camera-following volumetric DOT HAZE — 513 opaque pure-white 1x1/2x2 pixel specks fillin |
| `0x801113B4` | 3 | a03 | straightforward | CONFIRMED | 0F/0M/3m | A glowing, additive SCREEN-SPACE MOTION TRAIL / STREAK — the kind used for a weapon swipe, |


---

## 0x8010C1D8 — area 21 (A0L overlay)

**Portable:** straightforward — Every leaf already has a native owner: Math::rotmat/MeshQuads::rotmat (0x80085480), Math::matColScale (0x80084520), projComposeCamera+SpriteAnchor::baseScale (0x800329E0), cam.project+SpriteAnchor::otKeyInRange (0x800317CC), Render::altSpriteEmit (0x800328EC). The 36-byte record walk is byte-for-byte the format Render::meshQuadRecordsEmit already implements (uv0/uv1/uv2 words, RGB0..3 with the s8 model Z in byte 3, s8 x/y at +28..+35, terminator = word1 bit31, U bias), and the sprite tail is Render::fxRotSpriteTailRender with two constants changed. The port is ~60 lines of new producer plus four small parameter additions to existing shared helpers. Nothing needs a new RE step, nothing needs a guest-visible behavior, and nothing needs a tap.

**Effect:** A single node drawing two co-located layers in area 21 (the area Tomba rides a bird through, per its already-ported sibling FUN_8010C7F4): (1) a small, freely-ORIENTED, non-uniformly SQUASHABLE textured mesh — its rotation comes from three per-node Euler angles and each matrix column has its own scale factor, so the node can stretch the mesh along one axis; every quad is gouraud-shaded and SEMI-TRANSPARENT (command 0x3E, forced, the data's own semi bit discarded), and all four U coordinates scroll 32 texels per animation frame; (2) a camera-facing BILLBOARD at a second, independent anchor (node+0x60), uniformly scaled by depth, whose sprite model is picked out of a 6-frame table by the SAME animation frame index. Semi-transparent + U-scrolling + squashable + a 6-frame paired billboard reads as a flowing/gaseous element — a jet, gust, spray or beam with a puff or flare at its head — which fits its structural twin FUN_8012D9E8 in A01 being the water jet's controller. The +/-1024-unit model extent (s8 * 8, two orders of magnitude smaller than the * 256 the dust and narration-swirl emitters use through the same walker) says it is a SMALL, close-range element rather than a scenery-scale one. The whole thing is suppressed until the A0L phase byte reaches 4, so it belongs to a later stage of a scripted sequence, not to the ambient scene.

**Summary:** `overlay guest 0x8010C1D8` (authenticated executable/overlay evidence, first insn `addiu sp,-112` = 0x27BDFF90) is a TWO-PART node render fn and is the A0L twin of `FUN_8012D9E8` (A01), whose sprite tail is already ported as `Render::fxRotSpriteTailRender`. Part 1 is an inline ROTATED-MESH pass: it builds the node's own object matrix (`Math::rotmat` from the three s16 at node+0x54, then `Math::matColScale` by the three s16 at node+0x68/0x6A/0x6C), composes it with the pure scene camera (three MVMVA column composes + MVMVA of the node position at node+0x2E/0x32/0x36 plus the camera translation), then walks a FIXED 36-byte record table in overlay data at 0x801154E0 — the very same record format `Render::meshQuadRecordsEmit` already owns — RTPT'ing three corners + RTPS'ing the fourth, gating each quad on `AVSZ4 OTZ - 100` in the OT-key range [4,2047], and hand-linking a 52-byte POLY_GT4 (code 0x3E, semi-transparent, len 12) into `*0x800ED8C8`. Part 2 is the sprite tail: `FUN_800329E0(6)`, RTPS the packed anchor at node+0x60/+0x64 through `FUN_800317CC(-100)`, scale the published MAC0 by `(s16)node+0x70 >> 11` into BOTH scratchpad axis slots, then `FUN_800328EC` with a model list picked out of one of two 6-entry tables (0x8011587C when `u8 node+3 == 8`, else 0x80115864) indexed by the SIGNED HIGH BYTE of `u16 node+0x40`. That same signed byte, times 32, is the U-scroll bias applied to all four U bytes of every mesh quad — one animation frame index drives both halves. The whole function is gated at entry on `u8 *0x800BFA95 >= 4` (an A0L overlay phase counter); below that it draws nothing at all. Two things differ from every sibling in the family and are the silent-pixel-bug traps: the mesh vertex scale is signed-byte × 8 (NOT the × 256 `meshQuadRecordsEmit` hardcodes for fx_dust / narration_swirl), and the mesh's own OT gate has NO `if (key < 4) key = 4` clamp, unlike `SpriteAnchor::otKeyInRange`.


### Verifier corrections (CORRECTED)

- **[FATAL]** claim: The entry gate reads `u8 *0x800BFA95` (named kA0lPhase in globals/proposedWrappers, repeated in the summary, algorithm steps 1 and 21, and step (a) of the NATIVE PRODUCER SHAPE).

  actual: The instruction stream is `c->r[2] = (uint32_t)32780u << 16; c->r[2] = (uint32_t)c->mem_r8((c->r[2] + (uint32_t)-1451));` (ov_a0l_shard_1.c:1417/1430 and again at :1704/1706). 32780 = 0x800C, so the base is 0x800C0000; 1451 = 0x5AB; 0x800C0000 - 0x5AB = **0x800BFA55**, not 0x800BFA95. The spec's own parenthetical ('0x800C0000 << 16 form, -1451') states the right operands and then reports the wrong sum — off by 0x40. Verified with python: (32780<<16)-1451 = 0x800bfa55. Every other address in the spec re-derives correctly (0x800BF544, 0x800ED8C8, 0x801154E0, 0x8011587C, 0x80115864, 0x1F8000F8, 0x1F80010C all confirmed).

- **[MAJOR]** claim: NATIVE PRODUCER SHAPE step (e): `numerX = numerY = (u16)node+0x70`, carried in `AltSprite { uint32_t numerX, numerY }`.

  actual: The gen reads it SIGNED: `c->r[3] = (uint32_t)(int16_t)c->mem_r16((c->r[16] + (uint32_t)16));` then `(int64_t)(int32_t)MAC0 * (int64_t)(int32_t)r3`, `>> 11`. The spec's own algorithm step 26 and its nodeFields entry say s16 — step (e) contradicts them. Following step (e) turns a negative numerator into ~65536x the intended magnitude. This is the same latent defect already present in `Render::fxRotSpriteTailRender` (`a.numerX = (uint32_t)(uint16_t)c->mem_r16(node + kRotTailScale)`, fx_sprite.cpp:582) against its A01 twin, which also uses `(int16_t)` (ov_a01_shard_1.c, `mem_r16((c->r[17] + 16))` with an int16_t cast). AltSprite::numerX must become signed, or the port must not route through it.

- **[MINOR]** claim: emitContract.farColour: 'FUN_800328EC/FUN_8002847C run their colours through DPCS with IR0 = *0x1F800090, which this emitter never programs, so it is whatever the frame left.'

  actual: `guest 0x800328EC` (authenticated executable/overlay evidence) explicitly zeroes it: `c->r[2] = (uint32_t)8064u << 16; ... c->mem_w32((c->r[2] + (uint32_t)144), c->r[0]); guest 0x8002847C(c);` — 0x1F800000+144 = 0x1F800090, and it also passes a1=a2=0. IR0 is 0 BY CONSTRUCTION, not by leftover. The conclusion (ir0 = 0) is right; the stated reason is wrong and would mislead anyone auditing the family later.

- **[MINOR]** claim: risks[0]: 'THE RECORD TABLE AT 0x801154E0 CANNOT BE READ STATICALLY. It is A0L overlay DATA, not in authenticated executable/overlay evidence'

  actual: It IS in authenticated executable/overlay evidence The recorded binary evidence mis-scanned that data region as code and emitted pseudo-functions at exactly the record stride: overlay guest 0x80115474, _80115498, _801154BC (0x24 = 36 bytes apart — independent corroboration of the record size), then _80115504, _8011554C, _80115594 ... at 0x48. Their bodies are literal word dumps: `/* UNHANDLED op:0x30 raw=0xC02FD96E */`, `/* UNHANDLED special:0x34 raw=0x00B4B4B4 */` etc. So the table's UVs, cluts, colours and terminator bit are recoverable from the overlay bytes modulo the nop-elision problem (the recorded binary evidence emits nothing for 0x00000000 words, so byte offsets need care — overlay guest 0x801154BC accounts for 16 emitted instructions but spans 0x48 bytes, i.e. two zero words are elided). Notably a `raw=0xC02E0000` word sits a few slots past 0x801154E0 with BOTH bit31 (terminator) and bit30 set, so the run may be a single record — which would make the spec's 'count is DATA-DRIVEN and cannot be read statically … use kRecMax = 512' unnecessary. Resolve this before porting rather than shipping a 512-record guard over an unknown table.

- **[MINOR]** claim: algorithm step 4: 'Column scaling COMMUTES with the camera left-multiply that follows, so a host port may apply it either before or after the compose.'

  actual: True only in exact arithmetic. `guest 0x80084520` / `Math::matColScale` (gte_math.cpp:456) computes `(int32_t)((uint32_t)half * fac) >> 12` and stores it back with `& 0xffff` — WRAPPING, no clamp. The MVMVA compose that follows then clamps IR to 16 bits (lm=0) and the result is stored with `mem_w16` (truncated). `MeshQuads::composeScaled` (mesh_quads.cpp:91) does the reverse order and CLAMPS to [-32768,32767] between multiply and column-scale. On any element where a colScale product exceeds s16, the two orders give different pixels. The port should scale first, in the guest's order.

- **[MINOR]** claim: newHelpersNeeded / step (d): the mesh gate is reproducible by adding 'an optional gateBias' to meshQuadRecordsEmit plus an unclamped key map.

  actual: The gate input is the GTE's AVSZ4 output OTZ = (ZSF4 * (SZ0+SZ1+SZ2+SZ3)) >> 12, and this function never programs ZSF4 (CR30) — it writes only gte_write_ctrl(0..7). So the guest's bucket key depends on ambient scene state the producer does not own. The spec's risk list mentions the per-record depth-ordering judgement call but never names this dependency, and a host reimplementation of the gate that assumes a fixed ZSF4 will drop or keep the wrong quads. (The spec IS right that no >>2 is applied here — unlike SpriteAnchor::otKeyInRange, which takes SZ3 — so `otKeyRawInRange(key, bias)` correctly takes the key directly, and correctly omits otKeyInRange's `if (sz <= 0) return false` guard, which the guest also lacks.)

- **[MINOR]** claim: proposed whitelist entry and the six-entry sprite tables are stated as facts.

  actual: Two unmarked inferences: (1) `kSpriteTableN = 6` rests only on the 24-byte gap between 0x80115864 and 0x8011587C plus the A01 twin — the spec labels this an inference in risks but ships `bounds-check 0 <= frameIdx < 6` in the emitContract as if measured; (2) the guest performs NO bounds check at all on frameIdx before `table[frameIdx*4]`, so the proposed guard is a deliberate native divergence, not a reproduction. Worth an explicit in-code note (fxRotSpriteTailRender already carries the same unlabelled guard).


### Algorithm (as CORRECTED by the verifier — a DELTA; read with the ORIGINAL below)

1. ENTRY GATE. r21 = node (a0). r17 = node+0x60 and r18 = *(u32*)0x800BF544 (pool cursor) are loaded at entry, before the gate. Read u8 *(0x800C0000 - 1451) = **0x800BFA55** (NOT 0x800BFA95). If it is < 4, return immediately — no mesh, no sprite. Name the constant kA0lPhase = 0x800BFA55, kA0lPhaseMinDraw = 4.
2. FRAME INDEX. r3 = u16 at node+0x40; frameIdx = ((s32)(r3 << 16)) >> 24 = (s8)*(node+0x41); uBias = frameIdx << 5. Re-derived identically after the mesh loop for the sprite model-table index.
3. OBJECT ROTATION. typed runtime address dispatch 0x80085480(a0 = node+0x54, a1 = 0x1F800000) — the three s16 Euler angles at node+0x54/+0x56/+0x58 build the CR-packed 3x3 in scratchpad. Host: MeshQuads::rotmat.
4. COLUMN SCALE. Sign-extend the three s16 at node+0x68/+0x6A/+0x6C into s32 at sp+48/52/56, then typed runtime address dispatch 0x80084520(0x1F800000, sp+48): M[i][j] = (M[i][j] * f[j]) >> 12, TRUNCATED to 16 bits with no clamp. Apply the scale BEFORE the camera compose in the port too — the algebraic commutation does not survive the guest's wrap-vs-clamp difference (see errors).
5. CAMERA CRs. Load GTE CR0..CR4 from 0x1F8000F8/FC/100/104/108. CR5-7 are NOT loaded yet.
6. COMPOSE, COLUMN BY COLUMN. Three passes over halfword offsets (0,6,12), (2,8,14), (4,10,16) — columns 0, 1, 2. Each: IR1/IR2/IR3 <- column, MVMVA 0x4A49E012 (sf=1, mx=R, v=IR, cv=none, lm=0), IR1/IR2/IR3 stored back as u16 to the same three offsets. Net: M := Rcam * M.
7. COMPOSE TRANSLATION. SVECTOR at sp+64 from u16 reads at node+0x2E -> sp+64, node+0x32 -> sp+66, node+0x36 -> sp+68 (STRIDE 4). VXY0 = [sp+64], VZ0 = [sp+68]; MVMVA 0x4A486012 (sf=1, mx=R, v=V0, cv=none). MAC1/2/3 -> 0x1F800014/18/1C.
8. ADD CAMERA TRANSLATION. t[0..2] += *0x1F80010C / *0x1F800110 / *0x1F800114.
9. LOAD THE COMPOSED TRANSFORM. CR0..CR4 <- 0x1F800000..0x1F800010, CR5/6/7 <- 0x1F800014/18/1C. OFX/OFY/H/DQA/DQB and — critically — ZSF4 (CR30) are NOT programmed: the mesh pass inherits them from the frame's scene setup. A host reproduction of the OT gate must account for ZSF4 not being owned here.
10. MESH LOOP SETUP. r7 = 0x801154E0 (fixed overlay-data record table, NOT a node field), r5 = r7+24, r6 = poolCursor+48, r18 = poolCursor, vtxScale r19 = 8. DO-WHILE: at least one record is always processed. NOTE: the table IS decodable from the overlay bytes — the recorded binary evidence emitted it as pseudo-functions overlay guest 0x80115474/_80115498/_801154BC (36-byte stride) with the raw words in `UNHANDLED ... raw=0x...` comments. Decode it and pin the record count before porting instead of relying on a 512-record runaway guard.
11. PER RECORD, VERTEX UNPACK (rec = r5-24). Read r8 = u32[rec+4] FIRST. Four SVECTORs at sp+16/24/32/40, each component = (s8 byte) * 8 stored as s16: V0 = (rec+28, rec+30, rec+15), V1 = (rec+29, rec+31, rec+19), V2 = (rec+32, rec+34, rec+23), V3 = (rec+33, rec+35, rec+27).
12. RTPT 0x4A280030 on V0/V1/V2; FLAG (ctrl 31) -> sp+72; if FLAG < 0 skip to the loop tail (no packet, no cursor advance).
13. packet+8 = SXY0 (data 12), packet+20 = SXY1 (data 13), packet+32 = SXY2 (data 14).
14. RTPS 0x4A180001 on V3 (sp+40/+44); FLAG < 0 -> skip; packet+44 = SXY2.
15. AVSZ4 0x4B68002E; OTZ (data 7) -> sp+76. Then, in this exact order: k = OTZ - 100; k = ((s32)k >> ((k >> 10) & 31)) + (((s32)k >> 10) << 9); if !((u32)(k - 4) < 2044) k = -1; if k < 0 skip. NO min-4 clamp and NO sz<=0 test — an unclamped sibling of SpriteAnchor::otKeyInRange, taking the AVSZ4 key directly (no >>2).
16. PACKET FILL (52-byte POLY_GT4, offsets from the pool cursor). +4 = ([rec+12] & 0x00FFFFFF) | 0x3E000000; +12 = [rec+0]; +24 = [rec+4] & 0x007FFFFF (strips terminator bit31 and the data's own semi bit30 — semi is forced on by the 0x3E command); +36 = [rec+8]; +48 = (s32)[rec+8] >> 16; +16 = [rec+16]; +28 = [rec+20]; +40 = [rec+24].
17. U SCROLL. Byte-wrapping += uBias on the LOW byte of packet+12, +24, +36 and +48. V bytes untouched.
18. LINK. otSlot = *0x800ED8C8 + (k << 2); packet[0] = *otSlot | 0x0C000000; *otSlot = packetAddr; r6 += 52; r18 += 52.
19. LOOP TAIL. r5 += 36; r7 += 36; repeat while (s32)r8 > 0 — the record whose word at +4 is <= 0 signed is drawn and then ends the run.
20. AFTER THE LOOP. Store the advanced cursor back to *0x800BF544; r16 = node+0x60; re-derive frameIdx; re-test *0x800BFA55 < 4 (dead in practice — reproduce only the entry gate).
21. SPRITE TAIL — CAMERA + DQA. typed runtime address dispatch 0x800329E0(a0 = 6): pure scene camera into CR0..CR7, DQA = 6, DQB = 0.
22. SPRITE TAIL — ANCHOR + GATE. VXY0 = u32[node+0x60] (PACKED: X in bits 0-15, Y in bits 16-31), VZ0 = u32[node+0x64]. typed runtime address dispatch 0x800317CC(a0 = -100): returns 1 on FLAG<0 or SZ3<=0; else key = (SZ3>>2) + (-100), CLAMPED to >= 4, log-mapped, range-checked [4,2047]; on success publishes key/SXY2/MAC0 and returns 0. Non-zero -> return.
23. SPRITE TAIL — SCALE. s = ((s32)*0x1F800084 * (s32)(**int16_t**)[node+0x70]) >> 11 — multiply first, shift after, SIGNED numerator. Write s to both 0x1F800084 and 0x1F800088.
24. SPRITE TAIL — MODEL. table = ((u8)node[3] == 8) ? 0x8011587C : 0x80115864; rec0 = *(u32*)(table + frameIdx*4) — the guest applies NO bounds check; typed runtime address dispatch 0x800328EC(rec0), which ZEROES 0x1F800090 (IR0 = 0, cue is the identity) and then calls FUN_8002847C.
25. NATIVE PRODUCER SHAPE. (a) gate on *0x800BFA55 >= 4; (b) frameIdx = (s8)mem_r8(node+0x41), uBias = frameIdx*32; (c) MeshQuads::rotmat from the three s16 at node+0x54, column-scale by (s16 node+0x68/0x6A/0x6C) >> 12 IN THAT ORDER, Tobj = (s16) node+0x2E/0x32/0x36 (stride 4), projComposeObjectHost + projSetActive; (d) meshQuadRecordsEmit(0x801154E0, uBias, farBlack, ir0 = 0) with vtxScale = 8, semi forced on, and the UNCLAMPED per-record OT gate (bias -100) — but settle the ZSF4 dependency and the true record count first; (e) sprite tail via AltSprite with a PACKED anchor at node+0x60/+0x64, dqa = 6, gateBias = -100, depthBias = -100, shift = 11, and a SIGNED numer from (s16)node+0x70; fix altSpriteEmit's anchor stride (and the numer signedness) in the same change, since fxRotSpriteTailRender is currently wrong on both.

### Algorithm (ORIGINAL spec steps — the corrections above are stated against these)

1. ENTRY GATE. r21 = node (a0). Read u8 *0x800BFA95 (an A0L overlay phase byte, written all over ov_a0l_shard_*). If it is < 4, return immediately — no mesh, no sprite. (The same test is repeated verbatim after the mesh loop; it cannot change in between, so the second test is dead in practice.)
2. FRAME INDEX. r3 = u16 at node+0x40; r20 = ((s32)(r3 << 16) >> 24) << 5, i.e. frameIdx = (s8)*(node+0x41) and uBias = frameIdx * 32. Same frameIdx is reused at the very end as the sprite model-table index (r17).
3. OBJECT ROTATION. typed runtime address dispatch 0x80085480 (Math::rotmat) with a0 = node+0x54, a1 = 0x1F800000 -> writes a libgte MATRIX (5 CR-packed words of s16 at +0, s32 t[3] at +0x14) into scratchpad. The Euler angles are the three s16 at node+0x54/+0x56/+0x58.
4. COLUMN SCALE. Read three s16 at node+0x68, node+0x6A, node+0x6C, sign-extend each into a s32 at sp+48/+52/+56, then typed runtime address dispatch 0x80084520 (Math::matColScale) with a0 = 0x1F800000, a1 = sp+48 -> each element M[i][j] becomes (M[i][j] * f[j]) >> 12. Column scaling COMMUTES with the camera left-multiply that follows, so a host port may apply it either before or after the compose.
5. CAMERA CRs. Load GTE CR0..CR4 from scratchpad 0x1F8000F8/FC/100/104/108 (the pure scene-camera rotation). CR5-7 are NOT loaded here.
6. COMPOSE, COLUMN BY COLUMN. Three times: load column j of the scratchpad matrix into IR1/IR2/IR3 (data 9/10/11) from offsets (0,6,12) for j=0, (2,8,14) for j=1, (4,10,16) for j=2; run MVMVA 0x4A49E012 (func 0x12, sf=1, mx=0 = R, v=3 = IR, cv=3 = no translation, lm=0); read IR1/IR2/IR3 back and store them as u16 to the SAME three offsets. Result: scratchpad matrix = Rcam * (colScale(rotmat(angles))).
7. COMPOSE TRANSLATION. Build an SVECTOR at sp+64 from three u16 reads: node+0x2E -> sp+64, node+0x32 -> sp+66, node+0x36 -> sp+68. Load VXY0 = [sp+64], VZ0 = [sp+68] and run MVMVA 0x4A486012 (sf=1, mx=0 = R, v=0 = V0, cv=3 = NO translation). Store MAC1/MAC2/MAC3 (data 25/26/27) to 0x1F800014/18/1C (the MATRIX's t[3]).
8. ADD CAMERA TRANSLATION MANUALLY. t[0] += *0x1F80010C ; t[1] += *0x1F800110 ; t[2] += *0x1F800114. (These three are the scene camera's CR5-7 translation, kept in scratchpad.) Net effect of steps 3-8 is exactly Render::projComposeObjectHost(Robj = colScale(rotmat(node+0x54)), Tobj = (s16)node+0x2E/0x32/0x36).
9. LOAD THE COMPOSED TRANSFORM. Write GTE CR0..CR4 from 0x1F800000..0x1F800010 and CR5/6/7 from 0x1F800014/18/1C. OFX/OFY/H/DQA/DQB (CR24-27) are left at whatever the frame's scene setup programmed — the mesh pass does not touch them.
10. MESH LOOP SETUP. r7 = 0x801154E0 (record base, A0L overlay DATA, fixed — NOT read from the node), r5 = r7 + 24, r6 = poolCursor + 48, r18 = poolCursor = *0x800BF544, r19 = 8 (the vertex scale). The loop is a DO-WHILE: at least one record is always processed.
11. PER RECORD, VERTEX UNPACK (rec = r5 - 24 = r7). Read the loop-control word r8 = u32 [rec+4] FIRST (before any skip branch). Build four SVECTORs at sp+16, sp+24, sp+32, sp+40, each component = (s8 byte) * 8 stored as s16:  V0 = ([rec+28], [rec+30], [rec+15]),  V1 = ([rec+29], [rec+31], [rec+19]),  V2 = ([rec+32], [rec+34], [rec+23]),  V3 = ([rec+33], [rec+35], [rec+27]). The Z bytes are the TOP bytes of the four colour words at rec+12/16/20/24.
12. RTPT. Load VXY0/VZ0 = V0, VXY1/VZ1 = V1, VXY2/VZ2 = V2 (data 0..5), run RTPT 0x4A280030. Read FLAG (ctrl 31) to sp+72; if FLAG < 0 (any GTE error/overflow), SKIP this record (jump to the loop tail — the record is not drawn and no packet is allocated).
13. STORE THE FIRST THREE SCREEN POINTS. packet[+8] = SXY0 (data 12), packet[+20] = SXY1 (data 13), packet[+32] = SXY2 (data 14). (Written into the pool BEFORE the record is finally accepted; a later skip simply abandons those words without advancing the cursor.)
14. RTPS THE FOURTH VERTEX. Load VXY0/VZ0 = V3 (sp+40/+44), run RTPS 0x4A180001, re-check FLAG < 0 -> skip. packet[+44] = SXY2 (data 14).
15. AVSZ4 + OT GATE. Run AVSZ4 0x4B68002E, read OTZ (data 7) into sp+76. Then, IN THIS EXACT ORDER:  k = OTZ - 100;  k = ((s32)k >> ((k >> 10) & 31)) + (((s32)k >> 10) << 9);  if NOT ((u32)(k - 4) < 2044) then k = -1;  if k < 0 SKIP the record. NOTE: unlike FUN_800317CC / SpriteAnchor::otKeyInRange there is NO `if (k < 4) k = 4` clamp here, so a quad whose averaged depth lands below bucket 4 is DROPPED rather than pinned to the near bucket.
16. PACKET FILL (52-byte POLY_GT4, offsets relative to r18 = pool cursor). +4  = ([rec+12] & 0x00FFFFFF) | 0x3E000000  (rgb0 + code 0x3E = gouraud-textured 4-point, SEMI-TRANSPARENT, texture-blended). +12 = [rec+0] verbatim (u0 | v0<<8 | clut<<16). +24 = [rec+4] & 0x007FFFFF (u1 | v1<<8 | tpage<<16, masked to 23 bits — this STRIPS the terminator bit31 and the FUN_80027768-format semi bit30). +36 = [rec+8] verbatim (u2 | v2<<8, high half = uv3). +48 = (s32)[rec+8] >> 16 (u3 | v3<<8). +16 = [rec+16], +28 = [rec+20], +40 = [rec+24] verbatim (rgb1/rgb2/rgb3; their top bytes are the model Z bytes, which the GPU ignores as pad).
17. U SCROLL. Byte-wise (8-bit wrapping) add r20 = frameIdx*32 to the LOW byte of each of packet+12, +24, +36, +48, i.e. u0, u1, u2, u3. V bytes are untouched.
18. LINK. otSlot = *0x800ED8C8 + (k << 2); packet[+0] = *otSlot | 0x0C000000 (len = 12 words); *otSlot = packetAddr; r6 += 52; r18 += 52.
19. LOOP TAIL. r5 += 36; r7 += 36; if ((s32)r8 > 0) repeat. So records run until (and INCLUDING) the one whose word at +4 is <= 0 as a signed value — bit31 set, or zero.
20. AFTER THE LOOP. Store the advanced pool cursor back to *0x800BF544. Re-read node+0x40 -> r17 = frameIdx, re-read *0x800BFA95 and re-test < 4 (dead branch).
21. SPRITE TAIL — CAMERA + DQA. typed runtime address dispatch 0x800329E0 with a0 = 6: reload the pure scene camera CR0..CR7 from 0x1F8000F8..0x114 and set DQA = 6, DQB = 0 (depth-cue MAC0 becomes a per-Z pixel scale).
22. SPRITE TAIL — ANCHOR + GATE. r16 = node+0x60. Load VXY0 = u32 [node+0x60] (PACKED: X in lo16, Y in hi16) and VZ0 = u32 [node+0x64] (Z in lo16). typed runtime address dispatch 0x800317CC with a0 = -100. That leaf runs RTPS, returns 1 on FLAG<0 or SZ3<=0; otherwise key = (SZ3 >> 2) + (-100), CLAMPED to >= 4, then the same log map + [4,2047] range check; on failure it returns 1. On success it publishes key -> 0x1F800080, SXY2 -> 0x1F80008C, MAC0 -> 0x1F800084 and returns 0. If the return is non-zero, RETURN (no sprite).
23. SPRITE TAIL — SCALE. s = ((s32)*0x1F800084 * (s32)(s16)[node+0x70]) >> 11  — MULTIPLY FIRST, SHIFT AFTER. Write s to BOTH 0x1F800084 (scaleX) and 0x1F800088 (scaleY).
24. SPRITE TAIL — MODEL. table = (u8 [node+3] == 8) ? 0x8011587C : 0x80115864 ; rec0 = *(u32*)(table + frameIdx * 4). typed runtime address dispatch 0x800328EC(a0 = rec0) — the shared wrapper over the 36-byte FOUR-CORNER writer FUN_8002847C, i.e. Render::altSpriteEmit / emitAnimQuadRecords.
25. NATIVE PRODUCER SHAPE. (a) gate on *0x800BFA95 >= 4; (b) frameIdx = (s8)mem_r8(node+0x41), uBias = frameIdx*32; (c) MeshQuads::rotmat(c, s16 node+0x54, +0x56, +0x58, M) then apply the three column factors (s16 node+0x68/0x6A/0x6C) >> 12 to give float Robj; Tobj = (float)(s16)node+0x2E/0x32/0x36; projComposeObjectHost(Robj, Tobj, &w); projSetActive(&w); (d) meshQuadRecordsEmit(0x801154E0, uBias, farBlack, ir0 = 0) with vertex scale 8 and semi forced to 1, plus the per-record unclamped OT gate; projClearActive(); (e) the sprite tail via AltSprite { node, anchor = node+0x60 PACKED, dqa = 6, gateBias = -100, depthBias = -100, shift = 11, numerX = numerY = (u16)node+0x70, rec0 = table[frameIdx] } -> altSpriteEmit.

### Node fields

| offset | type | name | meaning |
|---|---|---|---|
| `+0x01` | u8 | visible | per-frame visibility marker checked by the render walk, not by this fn |
| `+0x03` | u8 | spriteTableSel | == 8 selects sprite model table A (0x8011587C); anything else selects table B (0x80115864) |
| `+0x0B` | u8 | nodeType | 0x20 = custom-render-fn node (the walk's dispatch condition) |
| `+0x18` | u32 | renderFn | == 0x8010C1D8 for this producer; the whitelist key |
| `+0x24` | u32 | next | next node in the walk's list |
| `+0x2E` | s16 | posX | mesh-pass world position X (read as u16 into an SVECTOR) |
| `+0x32` | s16 | posY | mesh-pass world position Y — NOTE the stride is 4, not 2 |
| `+0x36` | s16 | posZ | mesh-pass world position Z |
| `+0x40` | u16 | frameWord | its SIGNED HIGH BYTE (i.e. (s8)*(node+0x41)) is the animation frame index: drives BOTH the mesh U scroll (idx*32) and the sprite model-table index |
| `+0x54` | s16[3] | eulerAngles | +0x54/+0x56/+0x58 — the three angles handed to Math::rotmat (FUN_80085480) to build the object rotation |
| `+0x60` | u32 packed | spriteAnchorXY | sprite-tail world anchor, PACKED: X in bits 0-15, Y in bits 16-31 (i.e. s16 at +0x60 and +0x62) |
| `+0x64` | u32 (s16 in lo) | spriteAnchorZ | sprite-tail world anchor Z in the low halfword |
| `+0x68` | s16 | colScale0 | matColScale factor for matrix COLUMN 0 (12.12 fixed point, >> 12) |
| `+0x6A` | s16 | colScale1 | matColScale factor for matrix COLUMN 1 |
| `+0x6C` | s16 | colScale2 | matColScale factor for matrix COLUMN 2 |
| `+0x70` | s16 | spriteScaleNumer | sprite-tail scale numerator: scale = (MAC0 * this) >> 11, written to BOTH axis slots |

### Globals

| addr | type | name | meaning |
|---|---|---|---|
| `0x800BFA95` | u8 | kA0lPhase | MAIN RAM (0x800C0000 << 16 form, -1451). An A0L-overlay phase/stage counter written and read throughout ov_a0l_shard_0/1. THIS FUNCTION DRAWS NOTHING WHEN IT IS < 4 — the gate is at entry and repeated (dead) after the mesh loop. Other A0L sites compare it against 1, 2, 3 and 4, so it is a small ordered phase, not a boolean. |
| `0x800BF544` | u32 | kPktPoolPtr | MAIN RAM (32780<<16 - 2748). Shared packet-pool bump-allocator cursor. Read into r18 at entry, advanced 52 bytes per emitted mesh quad, written back after the loop. Same global widescreen_margin_quad.cpp / tile_grid_layer.cpp already name. |
| `0x800ED8C8` | u32 | kOtBasePtrPtr | MAIN RAM (32783<<16 - 10040). Word holding the live ordering-table base pointer; the OT slot is *0x800ED8C8 + key*4. |
| `0x801154E0` | record[] | kMeshRecords | A0L OVERLAY DATA (32785<<16 + 21728). The fixed 36-byte mesh quad-record table this fn walks. NOT read from the node — every node with this render fn draws the same mesh, differing only by transform and frame index. Terminated by the record whose word at +4 is <= 0 signed. |
| `0x8011587C` | u32[6] | kSpriteModelTableA | A0L OVERLAY DATA (32785<<16 + 22652). Sprite-tail record-list pointers, selected when u8 node+3 == 8. Six entries (its distance from table B is 24 bytes, matching the 6-entry pair the A01 twin uses). |
| `0x80115864` | u32[6] | kSpriteModelTableB | A0L OVERLAY DATA (32785<<16 + 22628). Sprite-tail record-list pointers, the default table. |
| `0x1F800000` | libgte MATRIX | kScratchMatrix | SCRATCHPAD (8064<<16 + 0). The scratch object matrix: 3x3 s16 CR-packed in 5 words at +0..+0x10, s32 t[3] at +0x14/+0x18/+0x1C. Written by rotmat, matColScale, the three MVMVA column composes and the translation compose. |
| `0x1F8000F8` | u32[5] | kCamRot | SCRATCHPAD (8064<<16 + 248). Pure scene-camera rotation in CR0-4 packing. Read here for the object compose and again by FUN_800329E0 for the sprite tail. |
| `0x1F80010C` | s32[3] | kCamTrans | SCRATCHPAD (reached as 8064<<16 + 208, then +60/+64/+68). Scene-camera translation, added manually to the composed t[3] because the MVMVA uses cv=3 (no translation). |
| `0x1F800080` | s32 | kPubOtKey | SCRATCHPAD. OT key published by FUN_800317CC (also its scratch slot during the gate). |
| `0x1F800084` | s32 | kPubScaleX | SCRATCHPAD (read/written here as 0x1F800000 + 132). MAC0 published by FUN_800317CC; this fn multiplies it by (s16)node+0x70 and stores the result back. |
| `0x1F800088` | s32 | kPubScaleY | SCRATCHPAD (0x1F800000 + 136). Y pixel scale — this emitter writes the SAME value as X (uniform sprite scale). |
| `0x1F80008C` | u32 | kPubSxy2 | SCRATCHPAD. Screen anchor published by FUN_800317CC on success. |

### Callees

| addr | role | native equivalent |
|---|---|---|
| `0x80085480` | Math::rotmat(a0 = node+0x54 angle vector, a1 = 0x1F800000 dest MATRIX) — builds the object rotation from three Euler angles. | MeshQuads::rotmat(c, ax, ay, az, int32 M[3][3]) — game/render/mesh_quads.h. Host-only, no guest write. (Math::rotmat at game/math/gte_math.cpp:220 also owns this address but writes guest memory, so a read-only producer must use the MeshQuads form. NOTE: codemap flags 0x80085480 as DUAL-OWNERSHIP between gte_math.cpp and mesh_quads.cpp — pre-existing, not introduced here.) |
| `0x80084520` | Math::matColScale(a0 = 0x1F800000, a1 = sp+48 three s32 factors) — per-COLUMN 12.12 scale of the matrix in place. | Math::matColScale (game/math/gte_math.cpp:456) exists but WRITES GUEST MEMORY. Host equivalent is the tail of MeshQuads::composeScaled (out[i][j] = (s * colScale[j]) >> 12). Needs either a small MeshQuads::colScale(M, f) or composeScaled(M, identity, f, out). |
| `0x800329E0` | Sprite-tail setup: reload the pure scene camera into CR0-7 and set DQA = 6, DQB = 0 so the depth-cue MAC0 becomes a per-Z pixel scale. | Render::projComposeCamera(&cam) + SpriteAnchor::baseScale(cam.H, sz, 6). Already used by altSpriteEmit. (The guest fn itself is only an ORPHAN leaf, leaf_800329E0 in game/core/field_owned_leaves.cpp:1307.) |
| `0x800317CC` | Sprite-tail gate: RTPS the anchor in data regs 0/1, return 1 on FLAG<0 or SZ3<=0, else key = (SZ3>>2) + a0, clamp >= 4, log-map, range-check [4,2047]; on success publish key/SXY2/MAC0 and return 0. Called with a0 = -100. | EObjXform::project + SpriteAnchor::otKeyInRange(pv.sz, -100) — game/render/fx_sprite.h. Exact match INCLUDING the min-4 clamp. (Guest side is ORPHAN leaf_800317CC, field_owned_leaves.cpp:1260.) |
| `0x800328EC` | Sprite-tail writer wrapper: consumes the published anchor/scale and walks the 36-byte four-corner record list at a0. | Render::altSpriteEmit (game/render/fx_sprite.cpp:498) -> emitAnimQuadRecords. Already LIVE and used by three producers. |
| `0x8002847C` | Reached indirectly through 0x800328EC — the 36-byte four-corner record writer itself. | emitAnimQuadRecords (static in game/render/fx_sprite.cpp). codemap reports NO owner for the address, but the behaviour is fully owned by that static; nothing new is needed. |

### Emit contract

- **dqa:** MESH PASS: none — DQA/DQB are not programmed, MAC0 is unused, and the four corners are real projected 3-D vertices. SPRITE TAIL: DQA = 6, DQB = 0 (FUN_800329E0(6)).
- **farColour:** MESH PASS: no depth cue at all — the four RGB words go into the packet raw (only rgb0 is masked, to make room for the 0x3E command byte). Native call therefore uses ir0 = 0 with a zero far colour, which is the identity in meshQuadRecordsEmit's depthCue. SPRITE TAIL: FUN_800328EC/FUN_8002847C run their colours through DPCS with IR0 = *0x1F800090, which this emitter never programs, so it is whatever the frame left — altSpriteEmit's existing default (ir0 = 0, no far colour) is the right reproduction, matching every other 0x800328EC producer.
- **gateBias:** MESH PASS: -100, applied to the AVSZ4 OTZ, and WITHOUT the min-4 clamp: k = OTZ - 100; k = (k >> ((k>>10)&31)) + ((k>>10) << 9); reject unless (u32)(k-4) < 2044. SPRITE TAIL: -100 passed to FUN_800317CC, which DOES clamp to >= 4 before the log map. (Contrast the A01 twin FUN_8012D9E8, which gates with 0 and subtracts 100 AFTER the log map.)
- **ir0Formula:** MESH PASS: ir0 = 0 (no DPCS/DPCT anywhere in the loop). SPRITE TAIL: not programmed by this emitter; use altSpriteEmit's identity default.
- **perFrameCount:** MESH PASS: one POLY_GT4 per record in the table at 0x801154E0 that passes both the RTPT/RTPS FLAG test and the OT range gate; the record run is do-while, terminated by (and including) the first record whose word at +4 is <= 0 signed. Count is DATA-DRIVEN and cannot be read statically — no captured RAM dump in scratch/ has A0L resident. Use meshQuadRecordsEmit's existing kRecMax = 512 runaway guard. SPRITE TAIL: at most ONE anchor, emitting the whole four-corner record list at table[frameIdx].
- **recordListFormula:** MESH PASS: rec_i = 0x801154E0 + 36*i — a FIXED overlay-data table, not a node field. SPRITE TAIL: table = (u8 mem_r8(node+3) == 8) ? 0x8011587C : 0x80115864 ; rec0 = mem_r32(table + frameIdx*4), frameIdx = (s8)mem_r8(node+0x41), 6 entries per table (bounds-check 0 <= frameIdx < 6 as fxRotSpriteTailRender does).
- **scaleFormula:** MESH PASS: model vertex components = (s8 record byte) * 8, stored as s16. THIS IS THE TRAP — meshQuadRecordsEmit currently hardcodes * 256 for its fx_dust / narration_swirl callers. SPRITE TAIL: scaleX = scaleY = ((s32)MAC0 * (s32)(s16)node+0x70) >> 11 — MULTIPLY FIRST, SHIFT AFTER (opposite of FUN_80113768's shift-then-multiply).
- **writer:** TWO writers in one fn. (1) MESH PASS: an INLINE hand-built 52-byte POLY_GT4 (code 0x3E = gouraud-textured 4-point, semi-transparent, texture-blended; OT tag length 0x0C = 12 words) linked into *0x800ED8C8 + key*4 out of the shared pool at *0x800BF544. Its record format is byte-identical to FUN_80027768's, which Render::meshQuadRecordsEmit already owns. (2) SPRITE TAIL: FUN_800328EC -> FUN_8002847C, the 36-byte FOUR-CORNER record writer = Render::altSpriteEmit / emitAnimQuadRecords.

### Helpers still needed

- `n/a (host-side, game/render/mesh_quads.cpp:105)` (~4): Render::meshQuadRecordsEmit needs a VERTEX SCALE parameter. It hardcodes `(int8_t)byte * 256`; this emitter uses * 8 (r19). Add `int vtxScale = 256` as a defaulted trailing parameter and use it in the sb() lambda. Getting this wrong is a silent 32x geometry-size bug.
- `n/a (host-side, game/render/mesh_quads.cpp:105)` (~3): Render::meshQuadRecordsEmit needs a SEMI override. It derives semi from word1 bit30 (kSemiBit, the FUN_80027768 convention); this emitter MASKS word1 with 0x007FFFFF and hardcodes command 0x3E, i.e. semi is ALWAYS on and the data bit is discarded. Add `int semiForce = -1` (-1 = keep the bit30 rule, 1 = force on).
- `n/a (host-side, game/render/mesh_quads.cpp:105)` (~8): Render::meshQuadRecordsEmit needs the PER-RECORD OT RANGE GATE it currently omits (it draws every record). Add an optional `int gateBias` and, when supplied, skip a record whose averaged view depth fails the UNCLAMPED map. Pairs with the next item.
- `n/a (host-side, game/render/fx_sprite.h / fx_sprite.cpp)` (~6): SpriteAnchor needs an UNCLAMPED sibling of otKeyInRange — call it otKeyRawInRange(int key, int bias) — reproducing the inline gate exactly: k = key + bias; k = (k >> ((k>>10)&31)) + ((k>>10) << 9); return (u32)(k-4) < 2044. The existing otKeyInRange pins k to >= 4 first, which turns 'drop the near quad' into 'draw it in the near bucket'. Both variants are real and must coexist.
- `n/a (host-side, game/render/render.h:285 / fx_sprite.cpp:498)` (~6): Render::AltSprite must support a PACKED anchor. altSpriteEmit reads the anchor as three s16 at anchorX+0/+4/+8, which is right for the 0x2E/0x32/0x36 layout but WRONG for the packed VX|VY at node+0x60 with VZ at node+0x64 that this fn (and the already-ported fxRotSpriteTailRender) uses. Add `bool anchorPacked = false` (or an explicit `anchorStride`). THIS IS A LIVE BUG IN fxRotSpriteTailRender, which reads node+0x60/+0x64/+0x68 instead of +0x60/+0x62/+0x64 — it has never been caught because that producer emits zero times across the whole replay library.
- `n/a (host-side, game/render/mesh_quads.h)` (~6): A column-scale-only helper: MeshQuads::colScale(const int32_t M[3][3], const int32_t f[3], float out[3][3]) doing out[i][j] = (M[i][j] * f[j]) >> 12. composeScaled already contains this expression but only as the tail of a multiply; this fn scales a single matrix.
- `0x8010C1D8` (~6): render_walk.cpp whitelist entry, with the same overlay-residency guard the siblings use: `else if (rfn == 0x8010C1D8u && c->mem_r32(0x8010C1D8u) == 0x27BDFF90u)` (addiu sp,-112).

**Guest writes:** "The guest-visible behavior writes guest memory in six places, and a read-only native producer can skip EVERY one of them because the whitelist dispatch in render_walk.cpp is purely ADDITIVE — the guest-visible behavior still executes underneath during the guest's own walk, so all of this state is produced regardless. (1) Scratchpad MATRIX at 0x1F800000..0x1F80001F: rotmat's 5 packed words, matColScale's rewrite of the same 5, the three MVMVA column write-backs (u16 stores at +0/+2/+4, +6/+8/+10, +12/+14/+16), the translation stores at +0x14/+0x18/+0x1C and the read-modify-write that adds the camera translation. (2) Guest stack sp-112..sp-1: the four SVECTORs at sp+16/24/32/40, the s32 scale triple at sp+48/52/56, the position SVECTOR at sp+64, the FLAG copy at sp+72, the OT key at sp+76, and the r16-r21/r31 spills at sp+80..104. A pc_render producer never runs on the guest stack, so no frame mirroring is required (this is NOT an owned-leaf port — see CLAUDE.md's guest-stack rule, which applies to overrides, not to display-pass overlays). (3) The 13-word POLY_GT4 packets in the shared pool, plus the pool cursor at 0x800BF544. (4) The OT link word at *0x800ED8C8 + key*4. (5) FUN_800317CC's publications at 0x1F800080 / 0x1F800084 / 0x1F80008C, and this fn's own overwrite of 0x1F800084 and 0x1F800088 with the scaled value. (6) Whatever FUN_800328EC / FUN_8002847C write for the sprite tail. The native port must perform NONE of these: it composes in host floats, emits through RenderQueue::drawWorldQuad, and reads the camera through projComposeCamera / projComposeObjectHost so it interpolates at fps60."


### Proposed wrappers

- An anonymous-namespace constant block in the style fx_sprite.cpp already uses, named for what each thing IS: kA0lPhase = 0x800BFA95 / kA0lPhaseMinDraw = 4; kMeshRecords = 0x801154E0; kSpriteTableA = 0x8011587C, kSpriteTableB = 0x80115864, kSpriteTableN = 6, kSpriteTableSelByte = 0x03, kSpriteTableSelVal = 8.
- Node-field constants (this node has TWO anchors and they are not interchangeable — name them so): kMeshAngles = 0x54 (3 x s16 Euler), kMeshPos = 0x2E (3 x s16, STRIDE 4), kMeshColScale = 0x68 (3 x s16 column factors), kFrameWord = 0x40 (frame index = its signed high byte), kSpriteAnchorPacked = 0x60 (VX|VY packed, VZ at +0x64), kSpriteScaleNumer = 0x70.
- Emitter-behaviour constants, one per number that a sibling gets different: kMeshVtxScale = 8 (vs 256 for fx_dust / narration_swirl), kMeshGateBias = -100, kUPerFrame = 32, kSpriteDqa = 6, kSpriteGateBias = -100, kSpriteDepthBias = -100, kSpriteShift = 11.
- SpriteAnchor::otKeyRawInRange(key, bias) as the named counterpart of otKeyInRange, with a comment stating the ONE difference (no min-4 clamp) and which emitters use which — so the next member of the family cannot pick the wrong one by accident.
- Render::meshQuadRecordsEmit gains a small trailing options struct or three defaulted params (vtxScale, semiForce, gateBias) rather than a second copy of the walk — it is documented as 'the ONE host-side walk of the packed-mesh quad-record format' and must stay that.
- Render::AltSprite gains `bool anchorPacked` with a comment naming the two layouts in the family (three s16 at stride 4 for the 0x2E form; VX|VY packed + VZ for the 0x60 form) — this is the field that makes the existing fxRotSpriteTailRender correct.
- A single producer method Render::fxRotMeshSpriteRender(uint32_t node) in game/render/fx_sprite.cpp (it is the sprite family's twin) or a new game/render/fx_rot_mesh.cpp, split into two named private steps — rotMeshPassEmit(node, frameIdx) and the existing altSpriteEmit call — so the two halves read as the two effects they are.
- A cfg_dbg("fxsprite") line matching the siblings' format: node, phase, frameIdx, uBias, quads drawn by the mesh pass, sprite anchor/sz/scale/quads — so the producer can be censused the same way the 22-area sweep censuses the others.

### Risks

- THE RECORD TABLE AT 0x801154E0 CANNOT BE READ STATICALLY. It is A0L overlay DATA, not in authenticated executable/overlay evidence, and no 2 MB dump in scratch/ has A0L resident (I scanned every >= 2 MB .bin for 0x27BDFF90 at file offset 0x10C1D8 — zero hits, consistent with the findings note that 0x8010C1D8 is absent from every captured dump). So the record COUNT, the actual UVs/colours, whether word1 bit30 is set, and the true size of the two 6-pointer sprite tables are all unverified. The 6-entry table size is inferred from the 24-byte gap between 0x80115864 and 0x8011587C plus the A01 twin's identical pair; it is an inference, not a measurement.
- WHAT THE EFFECT LOOKS LIKE IS A GUESS. I can state the mechanism exactly but not the picture. Everything visual below the mechanism level (the effectGuess field) is inference from the shape, not from a screenshot.
- THE MESH PASS'S PER-RECORD DEPTH ORDERING IS A JUDGEMENT CALL. The guest puts the whole quad in ONE OT bucket from AVSZ4(OTZ) - 100; a native producer with a real depth buffer naturally uses per-vertex depth. meshQuadRecordsEmit already made that choice for fx_dust / narration_swirl. Whether to additionally apply the -100 bucket bias (= -400 view units, the same 4x conversion altSpriteEmit uses for its authored near bias) is not decidable from instruction-level analysis — it is authored depth, so I lean toward applying it, but it is exactly the kind of thing that only a pixel A/B settles.
- THE FLAG < 0 SKIP HAS NO EXACT NATIVE ANALOGUE. GTE FLAG bit31 aggregates MAC overflow, SZ3 saturation and divide overflow; in float projection none of those exist. The closest faithful reproduction is 'skip the quad if any of the four vertices projects at or behind the near plane'. This is the same approximation every existing native producer makes, but it is an approximation.
- THE EXISTING fxRotSpriteTailRender IS PROBABLY WRONG ABOUT ITS ANCHOR and this port would inherit the bug via altSpriteEmit. altSpriteEmit reads three s16 at stride 4; the packed node+0x60 layout is X at +0x60, Y at +0x62, Z at +0x64. I am confident about the guest side (gte_write_data(0, u32 at node+0x60) is VXY0 = X|Y<<16 and gte_write_data(1, u32 at node+0x64) is VZ0 — the same packing the FUN_80027A4C family's kAnchorXY/kAnchorZ pair uses). It has gone unnoticed because that producer emits zero times across the whole 15-replay library. Fix it in the same change, and re-check anything that cited fxRotSpriteTailRender as working.
- THE SECOND *0x800BFA95 < 4 TEST after the mesh loop is dead as far as instruction-level analysis can tell (nothing in the loop can write that byte), but it is reproduced from the instruction stream, not proven unreachable. Reproducing only the entry gate is the right call; noting it here in case a future reader wonders why the port has one test and the guest-visible behavior has two.
- 0x800BFA95's SEMANTICS ARE ONLY PARTLY KNOWN. I can prove it is a small ordered phase counter local to the A0L overlay (compared against 1, 2, 3 and 4 at different sites, written from several) and that this fn requires >= 4. What the phases MEAN in the area-21 sequence is not established, so the constant should be named for the gate (kA0lPhaseMinDraw) and not for a guessed meaning.
- THE MESH VERTEX SCALE OF 8 IS UNUSUAL and worth double-checking against the other unported member of this shape (0x8013B118, A04) before generalising meshQuadRecordsEmit's new parameter — s8 * 8 gives a +/-1024-unit model where the dust/swirl emitters give +/-32768. It is unambiguous in the instruction stream (r19 = 8, set once outside the loop), but it means the mesh is two orders of magnitude smaller than the other users of the same walker, which is a good reason to sanity-check the picture rather than trust the number alone.


---

## 0x8013B118 — area 4 (A04 overlay)

**Portable:** moderate — Branch A is straightforward — it is the FUN_800328EC family this codebase already owns (`Render::altSpriteEmit` / `emitAnimQuadRecords`), needing only a new clamping OT-gate helper because its publisher is FUN_80032EB8, NOT FUN_800317CC. Branches B/C/tail are moderate: the mesh writer FUN_80027768 is owned by `FxMesh::draw`, but only as a GUEST-TIME scoped tap that derives its transform by reading GTE control registers; a display-pass native producer needs an overload taking an explicit `EObjXform` (trivially built by `projComposeObjectHost(diag(byte<<2), anchor)`) and an explicit IR0 cue. The genuinely blocked piece is the 342-point field in `overlay guest 0x8013AD90`: it is a 218-line raw GP0 tile emitter with its own LCG, RTPS, pool-space guard and hand-rolled OT link, and it has no analogue anywhere in `game/render/` — that is a separate producer card, not part of this port. One hard constraint shapes the design: the mesh IR0 cue contains a `Rng::next()` dither and `Rng::next()` WRITES the guest seed at 0x80105EE8, so a read-only producer may not call it.

**Effect:** An AREA-4 AMBIENT/ATMOSPHERE effect whose whole content switches on story progress. Before phase 44 it is (a) a field of 342 pure-white GP0 tile points, 2x2 px when SZ3 < 1536 and 1x1 beyond — i.e. snow / sparkles / dust motes / distant stars — scattered by an LCG around the scratchpad camera and re-randomised every frame, plus (b) a small cluster of textured mesh panels around world (4000..5600, -7400..-9300, 6800..9700) with the texture's U column scrolling 4 units/frame and wrapping at 64, which is the classic PSX way to animate FLOWING WATER / STEAM / a light shaft; the panels fade to black over ~20 frames when `node[5] == 0` and fade back in over 32 frames when it is non-zero, so the effect is switched on and off by whatever owns node[5]. After phase 44 the meshes are gone and it is instead nine large fixed billboards (scale 3x, three model variants) spread over Z 4532..24114 plus a 64-sprite figure-eight trail with random Z spread over 18390 world units — a long swarm/stream of small billboards down a corridor. The mesh scale factors are tiny (0.078 and 0.031 in 1.3.12) against corner coordinates that are `s8 * 256`, so the panels are roughly 20 and 8 world units per corner unit — small props, not scenery. LOW confidence on the visual noun; HIGH confidence on the mechanics above.

**Summary:** `overlay guest 0x8013B118` (overlay A04, area 4, ~412 lines, frame 216 bytes) is a FOUR-BRANCH ambient-effect controller for one type-0x20 node (0x800EDC90 in `scratch/raw/c18_a4.bin`, `node+0x18 == 0x8013B118`, vis=1). It opens with the sprite-family standard `FUN_800329E0(6)` — pure scene camera into GTE CR0-7, DQA=6, DQB=0 — then forks on the MAIN.EXE story-phase byte `*(u8*)0x800E7EAA`. **Branch A (phase >= 44)** is a pure `FUN_800328EC` sprite emitter: nine fixed world anchors copied out of overlay data at 0x8010A3EC (each `{s16 vx, vy, vz, modelIdx}`), each gated by `FUN_80032EB8(0)` and drawn at `scale = MAC0*3` on both axes; then SIXTY-FOUR more sprites whose anchor is a 2:1 Lissajous in XY (`VX = 19968 + (rcos(128i)*150>>12)`, `VY = -6078 + (rsin(64i)*150>>12)`) with a pseudo-random Z from a `x = 5x+123` LCG reduced by repeated `-18390` while `(u16)x >= 18390`, drawn at `scale = MAC0` on both axes with the record list cycling `table4[i & 3]`. **Branches B/C** (phase < 44) draw scrolling-U textured MESH panels through `FUN_80027768`, with the object transform composed by `FUN_800318A0` = `projComposeObjectHost(diag(byte<<2), anchor)`: branch B (bit 4 of `0x800BFE56` clear) first runs the big helper `overlay guest 0x8013AD90` — a 342-point white GP0-tile field it links by hand into the OT — then steps the fade `node+0x58` and the U-scroll `node+0x56` and draws six mesh lists from 0x8010A464 with `sortBias = -160`; branch C (bit 4 set) draws two copies of one 4-record mesh list (0x80143B4C) with `sortBias = 0` and an 8-step U-scroll advanced on odd frames only. **A common TAIL**, reached from both mesh branches, draws two more 6-record mesh lists (0x80143270, 0x80143198) at `sortBias = -100`, but only when the scratchpad area sub-phase `*(u8*)0x1F800207` is in {2,3,4}. Everything runs with the GTE far colour CR21-23 forced to zero, so every depth cue is a straight fade toward black. Branch A programs IR0 = 0 (identity cue); the mesh paths DRIVE IR0 from node state plus a `Rng::next()` dither.


### Verifier corrections (CORRECTED)

- **[MAJOR]** claim: Two independent globals: `0x800E7EAA` = STORY_PHASE (top fork, `< 44`) and `0x1F800207` = AREA_SUBPHASE, "Mirror of *(0x800E7E80+42)", gating the common TAIL to {2,3,4}.

  actual: 0x800E7E80 + 42 == 0x800E7EAA. The scratchpad byte IS the story-phase byte, mirrored by Pool::setupViewScroll (game/world/pool.cpp:240: `c->mem_w8(0x1F800207u, (uint8_t)c->mem_r8(S0 + 42))` with `S0 = 0x800E7E80` at line 180). Both read 1 in scratch/raw/c18_a4.bin, consistently. So the function forks on ONE variable twice: phase >= 44 -> branch A (no tail); phase < 44 -> mesh, and the tail draws only when that same phase is 2, 3 or 4. Consequences the spec gets wrong by construction: (a) the tail is structurally unreachable from branch A not merely because of control flow but because 44 > 4; (b) the proposed `A04Mode` enum wants ONE named story-phase accessor, not `storyPhaseAtLeast()` plus an unrelated `AREA_SUBPHASE`; (c) the acceptance-gate advice "drive 0x1F800207 to 2/3/4" is really "drive the story phase to 2/3/4" — and since the mirror is only republished at scene setup, poking one without the other desyncs them and makes the fork read two different values.

- **[MAJOR]** claim: nodeFields +0x56 uScroll: "Branch B advances it by 4/frame wrapping at 64".

  actual: Unconditionally false. The U-scroll step sits AFTER the fade step, and the fade step's `fade >= 4096` case branches to L_8013B690 (the tail gate), skipping the U-scroll store, the CR21-23 zeroing, the FUN_8009A450 call and the IR0 publish. So while the panels are fully faded out, node+0x56 FREEZES and the guest RNG is not stepped that frame. In the very capture the spec cites, node+0x58 == 4096, i.e. this is the state the node is actually in. The ordered algorithm steps convey it implicitly; the nodeFields prose states it as an unconditional per-frame advance and is wrong.

- **[MINOR]** claim: calls[0x8013AD90]: "builds a base angle triple into 0x1F8000C0 from the node's own +0x2C-ish deltas ... produces three 11-bit angle offsets".

  actual: The base triple is built on the helper's OWN STACK at sp+16/18/20, from the scratchpad camera words at 0x1F8000D2/0x1F8000D6/0x1F8000DA each minus 1024 plus `((s16)*(0x1F800104|106|108) << 11) >> 12`, then differenced against (s16)node+0x2C/+0x2E/+0x30 into r22/r21/r20. 0x1F8000C0 is not the base triple: it is the PER-POINT RTPS input VECTOR (VX at +0, VY at +2, VZ at +4), each component `((s32)lcg >> 16) + base` masked `& 2047`. These are 11-bit POSITIONS fed to VXY0/VZ0, not angle offsets. Also: 0x1F8000C0 is rewritten only on the ACCEPT path (L_8013B034), so a rejected point leaves the previous coordinates in place and the next iteration re-RTPSes the same point.

- **[MINOR]** claim: newHelpersNeeded[0x8013AD90] / portable: the 342-point field "has no analogue anywhere in game/render/".

  actual: The pool-cursor + OT-bucket hand-emit pattern has at least four existing natives against the very same globals: game/render/perobj_billboard.cpp (PKT_POOL_PTR = 0x800BF544, documented "packet-pool tail ... prepended into the OT bucket"), game/render/tile_grid_layer.cpp (kPktPoolPtr 0x800BF544 + kOtBaseGlobal 0x800ED8C8 + OT bucket splice), game/render/overlay_ground_gt3gt4.cpp, game/render/widescreen_margin_quad.cpp. Filing it as a separate card is still defensible on size, but the stated justification ("no analogue") is false and understates how much prior art the porter has.

- **[MINOR]** claim: calls[0x8009A450].nativeEquivalent: "Rng::next() (game/core/rng.h ...)".

  actual: The header is game/math/rng.h (`class Rng`, `SEED_ADDR = 0x80105EE8` at line 18). game/core/rng.h does not exist. The seed-write claim itself is correct.

- **[MINOR]** claim: nodeFields +0x58: "Depth-cue amount in [0,4096]".

  actual: Range is [0, 4284]. The guard tests `(s16)v < 4096` BEFORE adding 204, so a value of 4080 passes the guard and 4284 is stored and then published as the IR0 base that same frame (cueScale saturates it to 0 -> the six panels render fully black for one frame before the next frame's guard fires). A port that clamps the field to 4096 changes nothing visually here, but the stated bound is not what the stream does.

- **[MINOR]** claim: calls[0x800328EC]: "three instructions — zero 0x1F800090 then tail into FUN_8002847C(list, 0, 0)".

  actual: guest 0x800328EC (authenticated executable/overlay evidence) opens a 24-byte guest frame (`sp -= 24`), spills r31 at sp+16, zeroes 0x1F800090, `jal` (not a tail call) into guest 0x8002847C with a1 = a2 = 0, then restores and returns. The argument claim is right; "three instructions" and "tail" are not. Harmless for a read-only pc_render producer, but it is a stack frame the guest really pushes.

- **[MINOR]** claim: calls[0x8013AD90]: "a THREE-WORD GP0 packet is written by hand" and "an LCG ... stepped 3-4 times per point".

  actual: The packet is FOUR words / 16 bytes: tag at pool+0 (`*(OT+1024) | 0x03000000`), colour+command 0x60FFFFFF at pool+4, XY (SXY2) at pool+8, WH at pool+12; both the pool cursor r19 and the write cursor r6 advance by 16. "Three-word" is only the tag's length field. The LCG is stepped exactly 3 times per loop iteration (multiplier 0x7D2B89DD, `x = x*mult + 1`), with 4 additional steps before the loop.

- **[MINOR]** claim: summary: "the mesh paths DRIVE IR0 from node state plus a `Rng::next()` dither".

  actual: Branch C writes `*(0x1F800090) = 0` (via r21+144) and never calls FUN_8009A450 — a constant identity cue, no node state, no dither. Only branch B and the tail drive IR0. The `ir0Formula` field states this correctly, so the spec contradicts itself; the summary is the version a porter skims.

- **[MINOR]** claim: emitContract.scaleFormula: "Both feed spriteRecordsEmit/emitAnimQuadRecords".

  actual: Neither branch-A loop touches the 8-byte FUN_80027A4C format, so Render::spriteRecordsEmit (game/render/fx_sprite.cpp:144, render.h:324 — "the ONE host-side walk of the FUN_80027A4C 8-byte quad-record format") is not in play. Only emitAnimQuadRecords (the 36-byte four-corner walk) applies. The `writer` field gets this right; scaleFormula muddies it.

- **[MINOR]** claim: globals[0x800BF544]: "Read/written only by overlay guest 0x8013AD90".

  actual: 0x800BF544 is the game-wide packet-pool bump cursor with several existing native readers/writers (widescreen_margin_quad.cpp:337 writes it, tile_grid_layer.cpp/perobj_billboard.cpp/overlay_ground_gt3gt4.cpp read it). True only within this function's scope; as written it reads as a global fact and is false.


### Algorithm (as CORRECTED by the verifier — a DELTA; read with the ORIGINAL below)

1. ENTRY (frame): sp -= 216; spill r16..r22 at sp+184,188,192,196,200,204,208 and r31 at sp+212. r16 = node (a0), r18 = node+0x50, r19 = 0x1F800084, r20 = 0x1F800088, r21 = 0x1F800000 (assigned after the call below), r22 = 0x1F800090.
2. ENTRY (camera): call FUN_800329E0(6) — CR0-4 from 0x1F8000F8, CR5-7 from 0x1F80010C/0110/0114, DQA(CR27)=6, DQB(CR28)=0. Unconditional for every branch; the mesh branches later overwrite CR0-7 via FUN_800318A0 but leave DQA/DQB alone.
3. TOP FORK on the STORY PHASE byte at 0x800E7EAA (NOTE: the scratchpad byte 0x1F800207 that gates the tail in step 19 is a MIRROR OF THIS SAME BYTE — Pool::setupViewScroll publishes *(0x800E7E80+42) == *(0x800E7EAA) there. Model both with ONE named story-phase accessor). If phase < 44 goto MESH BRANCHES (step 12); else BRANCH A.
4. BRANCH A, setup: memcpy 16 bytes 0x8010A3DC -> sp+16 (four 36-byte-record-list pointers); memcpy 72 bytes 0x8010A3EC -> sp+32 (nine 8-byte anchors). CR21=CR22=CR23=0. *(u32*)0x1F800090 = 0.
5. BRANCH A, LOOP 1 — i = 0..8, cursor = sp+32 + 8*i: GTE VXY0 = *(cursor+0), VZ0 = *(cursor+4); call FUN_80032EB8(0); non-zero return -> skip this anchor.
6. BRANCH A, LOOP 1 emit: MAC0 = *(s32*)0x1F800084; modelIdx = (int16_t)*(cursor+6) (SIGNED); recList = *(u32*)(sp+16 + 4*modelIdx); s = (MAC0<<1) + MAC0 = MAC0*3; store s to BOTH 0x1F800084 and 0x1F800088; call FUN_800328EC(recList). Capture modelIdx sequence: 0,0,0,1,1,1,2,2,2.
7. BRANCH A, LOOP 2 setup: lcg = 5131; angleAcc = 0; i = 0. (The 19968 stored to sp+104 before the loop is dead.)
8. BRANCH A, LOOP 2 body, i = 0..63, angleAcc = 64*i: cx = rcos(angleAcc<<1) = rcos(128*i); VX = 19968 + ((cx*150) >> 12) with an ARITHMETIC shift, stored as u16 at sp+104. sy = rsin(angleAcc) = rsin(64*i); VY = -6078 + ((sy*150) >> 12), stored as u16 at sp+106. VZ = (u16)(lcg + 5131) at sp+108.
9. BRANCH A, LOOP 2 project/emit: GTE VXY0 = *(u32*)(sp+104), VZ0 = *(u32*)(sp+108) (sp+110 is stale stack, harmless — VZ0 is 16-bit). FUN_80032EB8(0); non-zero -> skip. Else recList = *(u32*)(sp+16 + 4*(i & 3)); copy *(0x1F800084) into 0x1F800088 unchanged (scale = MAC0 on both axes, NO multiply); FUN_800328EC(recList).
10. BRANCH A, LOOP 2 LCG step — ALWAYS runs, including on skipped iterations, and after the emit: r = lcg & 0xFFFF; lcg = 5*r + 123; then `while (((uint16_t)lcg) >= 18390) lcg -= 18390;` (NOT a modulo — compare is 16-bit, subtract is 32-bit; state legitimately reaches 82863 over the 64 items). i += 1; angleAcc += 64.
11. BRANCH A end: jump straight to the epilogue. The common TAIL is not reachable from this branch (and could not be anyway: the tail wants phase in {2,3,4} while this branch needs phase >= 44).
12. MESH FORK: if ((u16)*0x800BFE56 & 0x10) goto BRANCH C (step 18); else BRANCH B.
13. BRANCH B, prelude: call overlay guest 0x8013AD90(node) — the 342-point white-tile field. Unconditional within branch B, before any state stepping.
14. BRANCH B, fade step: if ((u8)node[5] == 0) { if ((s16)*(node+0x58) >= 4096) goto TAIL — AND NOTE this early-out ALSO skips the U-scroll step, the CR21-23 zeroing, the Rng call and the IR0 publish, so node+0x56 FREEZES and the guest RNG is NOT stepped that frame; else *(u16*)(node+0x58) = (u16)fade + 204 (value may reach 4284). } else { if ((s16)*(node+0x58) > 0) *(u16*)(node+0x58) = (u16)fade - 128. }
15. BRANCH B, U-scroll step: u2 = (u16)*(node+0x56) + 4; store u2; if ((s16)u2 >= 64) store (u16)(u - 60) instead. The POST-step value is what all six meshes receive as a3.
16. BRANCH B, cue: CR21=CR22=CR23=0; r = FUN_8009A450(); *(u32*)0x1F800090 = (s16)*(node+0x58) + (r & 511) — POST-step fade, computed ONCE, shared by all six meshes.
17. BRANCH B, tables + loop: memcpy 48 bytes 0x8010A434 -> sp+112 (six anchors) and 24 bytes 0x8010A464 -> sp+160 (six list pointers). For i = 0..5: FUN_800318A0(sp+112 + 8*i, node+0x5C, 0x800A1CCC); FUN_80027768(*(u32*)(sp+160 + 4*i), 0, -160, (s16)*(node+0x56)). Fall through to the TAIL.
18. BRANCH C: if ((u16)*0x1F80017C & 1) *(u16*)(node+0x56) = ((u16)*(node+0x56) + 32) & 248 (8-step scroll on odd ticks only). CR21-23 = 0; memcpy 16 bytes 0x8010A47C -> sp+16; *(u32*)0x1F800090 = 0; copy the three bytes at 0x8010A48C to sp+104..106. For i = 0..1: FUN_800318A0(sp+16 + 8*i, sp+104, 0x800A1CCC); FUN_80027768(0x80143B4C, 0, 0, (s16)*(node+0x56)). Fall through to the TAIL.
19. TAIL gate: p = (u8)*0x1F800207 — THE STORY PHASE MIRROR, same variable as step 3. Draw only when (u32)(p - 2) < 3, i.e. p in {2,3,4}; otherwise goto epilogue.
20. TAIL setup: copy 4 bytes 0x8010A490 -> sp+16 (three u8 column scales, 0xA0 each, read UNSIGNED by FUN_800318A0) and 8 bytes 0x8010A494 -> sp+24 (one anchor). CR21=CR22=CR23=0. FUN_800318A0(sp+24, sp+16, 0x800A1CCC) ONCE — the same transform serves both tail prims.
21. TAIL prim A: r = FUN_8009A450(); *(u32*)0x1F800090 = (s16)*(node+0x62) + (r & 127); FUN_80027768(0x80143270, 0, -100, 0).
22. TAIL prim B: r = FUN_8009A450() again; *(u32*)0x1F800090 = (s16)*(node+0x66) + (r & 127); FUN_80027768(0x80143198, 0, -100, 0).
23. EPILOGUE: restore r16..r22 and r31 from the spill slots; sp += 216; return.

### Algorithm (ORIGINAL spec steps — the corrections above are stated against these)

1. ENTRY (frame): sp -= 216; spill r16..r22 at sp+184,188,192,196,200,204,208 and r31 at sp+212. r16 = node (a0), r18 = node+0x50, r19 = 0x1F800084, r20 = 0x1F800088, r21 = 0x1F800000, r22 = 0x1F800090.
2. ENTRY (camera): call FUN_800329E0(6) — loads scene-camera CR0-4 from 0x1F8000F8 and CR5-7 from 0x1F80010C, sets DQA(CR27)=6, DQB(CR28)=0. Native equivalent: projComposeCamera(&cam) + SpriteAnchor::baseScale(cam.H, sz, 6). Runs UNCONDITIONALLY for every branch (branches B/C/tail then overwrite CR0-7 via FUN_800318A0).
3. TOP FORK: if ((u8)*0x800E7EAA < 44) goto MESH BRANCHES (step 12). Else fall through to BRANCH A.
4. BRANCH A, setup: memcpy 16 bytes 0x8010A3DC -> sp+16 (four 32-bit record-list pointers), and 72 bytes 0x8010A3EC -> sp+32 (nine 8-byte anchors). Write GTE CR21=CR22=CR23=0 (far colour black). Write *(u32*)0x1F800090 = 0 (IR0 = identity cue).
5. BRANCH A, LOOP 1 — nine fixed anchors, i = 0..8, cursor = sp+32 + 8*i: load GTE VXY0 = *(cursor+0) (packed s16 VX | s16 VY<<16) and VZ0 = *(cursor+4) (s16 VZ in the low half). Call FUN_80032EB8(0). If it returns non-zero, SKIP this anchor (no emit).
6. BRANCH A, LOOP 1 emit: read MAC0 = *(s32*)0x1F800084 (just published by the gate); modelIdx = (s16)*(cursor+6); recList = *(u32*)(sp+16 + 4*modelIdx) i.e. table4[modelIdx]; compute s = MAC0 + (MAC0<<1) = MAC0*3 (MULTIPLY, no shift); store s to BOTH 0x1F800084 and 0x1F800088; call FUN_800328EC(recList). Observed modelIdx values in the A04 image are 0,0,0,1,1,1,2,2,2.
7. BRANCH A, LOOP 2 setup: lcg = 5131; angleAcc = 0; i = 0. (A dead store of 19968 to sp+104 precedes the loop and is immediately overwritten.)
8. BRANCH A, LOOP 2 body, i = 0..63 with angleAcc = 64*i: cx = rcos(angleAcc<<1) i.e. rcos(128*i) via FUN_80083F50; VX = 19968 + (((cx*5)*15)<<1 >> 12) — algebraically (cx*150)>>12 with an ARITHMETIC shift, stored as u16 at sp+104. sy = rsin(angleAcc) i.e. rsin(64*i) via FUN_80083E80; VY = -6078 + ((sy*150)>>12), stored as u16 at sp+106. VZ = (u16)(lcg + 5131), stored at sp+108.
9. BRANCH A, LOOP 2 project/emit: GTE VXY0 = *(u32*)(sp+104), VZ0 = *(u32*)(sp+108) (only the low 16 bits are used; sp+110 is stale stack). Call FUN_80032EB8(0). If non-zero, skip the emit. Else recList = *(u32*)(sp+16 + 4*(i & 3)); read MAC0 = *(s32*)0x1F800084; write that SAME value to 0x1F800088 (0x1F800084 is left as published — scale is 1x MAC0 on both axes, NO multiply); call FUN_800328EC(recList).
10. BRANCH A, LOOP 2 LCG step — ALWAYS runs, including on skipped iterations, and AFTER the emit: r = lcg & 0xFFFF; lcg = 5*r + 123; then `do { cond = ((u16)lcg < 18390); lcg -= 18390; } while (!cond); lcg += 18390;` — i.e. `while (((u16)lcg) >= 18390) lcg -= 18390;`. NOTE this is NOT a modulo: the comparison is on the low 16 bits while the subtraction is 32-bit, so the state legitimately exceeds 18390 (simulated max 82863 over the 64 items). Reproduce literally. i += 1, angleAcc += 64; loop while i < 64.
11. BRANCH A end: jump straight to the epilogue — the common TAIL is NOT executed on this branch.
12. MESH FORK: if ((u16)*0x800BFE56 & 0x10) goto BRANCH C (step 19). Else BRANCH B.
13. BRANCH B, prelude: call overlay guest 0x8013AD90(node) — the 342-point white-tile field (see calls). This runs BEFORE any of the state stepping below and is unconditional within branch B.
14. BRANCH B, fade step: if ((u8)node[5] == 0) { v = (s16)*(node+0x58); if (v >= 4096) goto TAIL (skip ALL six meshes); else *(u16*)(node+0x58) = (u16)v + 204; } else { v = (s16)*(node+0x58); if (v > 0) *(u16*)(node+0x58) = (u16)v - 128; }.
15. BRANCH B, U-scroll step: u = (u16)*(node+0x56); u2 = u + 4; store u2; if ((s16)u2 >= 64) store u - 60 instead (= u2 - 64). The FINAL stored value is what all six meshes receive as their U bias.
16. BRANCH B, cue: GTE CR21=CR22=CR23=0; r = FUN_8009A450() (Rng::next()); *(u32*)0x1F800090 = (s16)*(node+0x58) + (r & 511). This is computed ONCE and shared by all six meshes. Note the node+0x58 read here is the POST-step value.
17. BRANCH B, tables: memcpy 48 bytes 0x8010A434 -> sp+112 (six 8-byte anchors) and 24 bytes 0x8010A464 -> sp+160 (six mesh record-list pointers).
18. BRANCH B, LOOP i = 0..5: FUN_800318A0(a0 = sp+112 + 8*i, a1 = node+0x5C, a2 = 0x800A1CCC); then FUN_80027768(a0 = *(u32*)(sp+160 + 4*i), a1 = 0, a2 = -160, a3 = (s16)*(node+0x56)). Fall through to the TAIL.
19. BRANCH C: if ((u16)*0x1F80017C & 1) { *(u16*)(node+0x56) = ((u16)*(node+0x56) + 32) & 248; } — an 8-step U-scroll advanced on ODD frame ticks only. GTE CR21-23 = 0; memcpy 16 bytes 0x8010A47C -> sp+16 (two anchors); *(u32*)0x1F800090 = 0 (identity cue); copy the three s8 at 0x8010A48C into sp+104..106 (column scales). LOOP i = 0..1: FUN_800318A0(a0 = sp+16 + 8*i, a1 = sp+104, a2 = 0x800A1CCC); FUN_80027768(a0 = 0x80143B4C, a1 = 0, a2 = 0, a3 = (s16)*(node+0x56)). Fall through to the TAIL.
20. TAIL gate: p = (u8)*0x1F800207; if ((u32)(p - 2) >= 3) goto epilogue — i.e. draw only when p is 2, 3 or 4.
21. TAIL setup: copy 4 bytes 0x8010A490 -> sp+16 (three u8 column scales, value 0xA0 = 160 each) and 8 bytes 0x8010A494 -> sp+24 (one 8-byte anchor). GTE CR21=CR22=CR23=0. Call FUN_800318A0(a0 = sp+24, a1 = sp+16, a2 = 0x800A1CCC) ONCE — the SAME transform is used for both tail prims.
22. TAIL prim A: r = FUN_8009A450(); *(u32*)0x1F800090 = (s16)*(node+0x62) + (r & 127); FUN_80027768(a0 = 0x80143270, a1 = 0, a2 = -100, a3 = 0).
23. TAIL prim B: r = FUN_8009A450() again; *(u32*)0x1F800090 = (s16)*(node+0x66) + (r & 127); FUN_80027768(a0 = 0x80143198, a1 = 0, a2 = -100, a3 = 0).
24. EPILOGUE: restore r16..r22, r31 from the spill slots; sp += 216; return.

### Node fields

| offset | type | name | meaning |
|---|---|---|---|
| `+0x05` | u8 | meshFadeDir | 0 = fade the mesh panels toward black at 204/frame until the fade hits 4096 (at which point the six meshes stop drawing entirely); non-zero = recover at 128/frame, floored at 0. Read only on branch B. Observed 0 in the area-4 capture. |
| `+0x0B` | u8 | nodeType | 0x20 — the walk-level custom-render-fn node type; the render walk reads this before dispatching. |
| `+0x18` | u32 | renderFn | 0x8013B118 — this function. The whitelist entry must guard on `c->mem_r32(0x8013B118u) == 0x27BDFF28u` (addiu sp,-216), the overlay-residency signature. |
| `+0x50` | u16 | (unused here) | 0x0098 in the capture. Neither this function nor its helper reads it; r18 = node+0x50 is only a base register for the +0x52..+0x66 fields below. |
| `+0x52` | s16 | pointCountMinus1 | Loop bound of the point field inside overlay guest 0x8013AD90: `for (i = n; i >= 0; i--)`, so n+1 points. Capture value 341 -> 342 points. |
| `+0x56` | u16 | uScroll | Texture U bias handed to FUN_80027768 as a3. Branch B advances it by 4/frame wrapping at 64; branch C sets it to (v+32)&248 on odd frame ticks (an 8-step cycle 0,32,...,224). A pc_render producer READS this — the guest-visible behavior owns the stepping. |
| `+0x58` | s16 | meshFadeToBlack | Depth-cue amount in [0,4096] driving the mesh IR0 (0 = full colour, 4096 = black). Also the branch-B early-out: >= 4096 skips all six meshes and jumps to the tail. In scratch/raw/c18_a4.bin it is exactly 4096, i.e. at that instant the six meshes are NOT drawn. |
| `+0x5C` | u8[3] | meshColumnScale | Three UNSIGNED bytes read by FUN_800318A0 (lbu, then <<2) as the per-column 1.3.12 scale of the object matrix. Capture value {0x50,0x50,0x50} -> 320/4096 = 0.078125 uniform. Branch B only. |
| `+0x62` | s16 | tailCueA | Base depth-cue value for the first tail mesh (0x80143270); IR0 = this + (Rng::next() & 127). |
| `+0x66` | s16 | tailCueB | Base depth-cue value for the second tail mesh (0x80143198); IR0 = this + (Rng::next() & 127). |

### Globals

| addr | type | name | meaning |
|---|---|---|---|
| `0x800E7EAA` | u8 | STORY_PHASE | MAIN.EXE global (stable, not overlay). `32782<<16 + 32426`. The top fork: >= 44 selects the sprite branch A, < 44 the mesh branches. Widely used elsewhere as the global scene/story-phase gate (see game/ai/beh_typed_variant_router.cpp:56). Value 1 in the area-4 capture, so branch A is currently unreachable there. |
| `0x800BFE56` | u16 | AREA_PROGRESS_BITS | MAIN.EXE global. `32780<<16 - 426`. Per-area collected/progress bitmask (game/world/placement.cpp:76, game/world/pool.cpp:309). Bit 4 (0x10) set selects mesh branch C, clear selects mesh branch B. Value 0x0000 in the capture -> branch B. |
| `0x1F800207` | u8 | AREA_SUBPHASE | Scratchpad. `8064<<16 + 519`. Mirror of *(0x800E7E80+42), published by Pool::setupViewScroll (game/world/pool.cpp:240). The common tail draws ONLY when this is 2, 3 or 4. Value 1 in the capture -> the tail is skipped there. |
| `0x1F80017C` | u16 | FRAME_TICK | Scratchpad frame counter (the one CLAUDE.md's pc_skip rule requires bumping). Branch C advances its U-scroll only when bit 0 is set, i.e. on odd ticks -> a 30 Hz step. |
| `0x1F800084` | s32 | SPR_SCALE_X | Scratchpad. `8064<<16 + 132`. The MAC0 the gate FUN_80032EB8 publishes; then overwritten by the emitter with the final X pixel scale. Native equivalent: SpriteAnchor::baseScale(H, sz, dqa). |
| `0x1F800088` | s32 | SPR_SCALE_Y | Scratchpad. `8064<<16 + 136`. Y pixel scale slot; both branch-A loops write it. |
| `0x1F800090` | s32 | DEPTH_CUE_IR0 | Scratchpad. `8064<<16 + 144` (written as r21+144 and r22+0 in the body). 0 = identity cue; positive fades record colours toward the GTE far colour, which is forced to (0,0,0) here, so it is a straight fade to black. Same slot fx_mesh.cpp calls kDepthCueScratch. |
| `0x1F800080` | s32 | OT_KEY | Scratchpad. `8064<<16 + 128`. FUN_80032EB8's OT-bucket output; consumed by the writer to link the packet. A native producer replaces it with real per-vertex depth (proj_pz_to_ord). |
| `0x1F80008C` | u32 | SPR_SXY2 | Scratchpad. `8064<<16 + 140`. The projected screen anchor the gate publishes. |
| `0x1F8000F8 .. 0x1F800114` | MATRIX | SCENE_CAM | Scratchpad pure scene camera: CR0-4 rotation at 0x1F8000F8..0x108, CR5-7 translation at 0x1F80010C/0110/0114. Read by both FUN_800329E0 and FUN_800318A0. Native equivalent: Fps60::sceneCam via projComposeCamera. |
| `0x1F800000` | MATRIX | SCRATCH_MATRIX | FUN_800318A0's working object matrix (5 rotation words + 3 translation words at +0x14/0x18/0x1C). A GUEST WRITE — a native producer computes it host-side instead. |
| `0x800A1CCC` | SVECTOR | ZERO_ANGLES | MAIN.EXE .data. `32778<<16 + 7372`. Passed as FUN_800318A0's a2 -> RotMatrix's angle vector. Dump value: the first three s16 are 0,0,0, so the base rotation is the IDENTITY and FUN_800318A0 degenerates to a pure per-column SCALE. (Do NOT read this as a matrix — the a0/a1 order of FUN_80085480 is (angles, matrix).) |
| `0x8010A3DC` | u32[4] | A04_SPRITE_LISTS | Overlay A04 data. `32785<<16 - 23588`. Four 36-byte four-corner record-list pointers = {0x80143B28, 0x80143B04, 0x80143AE0, 0x80143ABC}, each ONE record (terminal bit set on record 0). Indexed by the anchor's modelIdx in loop 1 and by `i & 3` in loop 2. |
| `0x8010A3EC` | struct[9] | A04_FIXED_ANCHORS | Overlay A04 data. `32785<<16 - 23572`. Nine 8-byte records {s16 vx, s16 vy, s16 vz, s16 modelIdx}: (16640,-9600,4532,0) (17536,-10048,8502,0) (18048,-7872,20693,0) (15232,-7744,11571,1) (15616,-9216,16311,1) (17344,-6272,17912,1) (16704,-7296,7735,2) (16704,-7232,15264,2) (15360,-6592,24114,2). |
| `0x8010A434` | struct[6] | A04_MESH_ANCHORS_B | Overlay A04 data. `32785<<16 - 23500`. Six 8-byte anchors, +6 halfword unused: (5568,-7380,8436) (5568,-7560,9616) (4733,-8351,8536) (4133,-9301,9726) (4032,-9011,8379) (4682,-8131,6869). |
| `0x8010A464` | u32[6] | A04_MESH_LISTS_B | Overlay A04 data. `32785<<16 - 23452`. Six FUN_80027768 record-list pointers = {0x80143564, 0x80143564, 0x801434F8, 0x801434F8, 0x801434B0, 0x80143468} with 3, 3, 3, 3, 2, 2 records = 16 quads. |
| `0x8010A47C` | struct[2] | A04_MESH_ANCHORS_C | Overlay A04 data. `32785<<16 - 23428`. Two 8-byte anchors: (5146,-7507,7808) and (5146,-7748,8576). |
| `0x8010A48C` | u8[3] | A04_SCALE_C | Overlay A04 data. `32785<<16 - 23412`. {0x20,0x20,0x20} = 32 each -> column scale 32<<2 = 128/4096 = 0.03125. Copied byte-wise to sp+104 and read UNSIGNED by FUN_800318A0. |
| `0x8010A490` | u8[3] | A04_SCALE_TAIL | Overlay A04 data. `32785<<16 - 23408`. {0xA0,0xA0,0xA0} = 160 each (UNSIGNED — FUN_800318A0 uses lbu) -> column scale 640/4096 = 0.15625. |
| `0x8010A494` | struct | A04_TAIL_ANCHOR | Overlay A04 data. `32785<<16 - 23404`. One 8-byte anchor (4261, -10079, 17423). |
| `0x80143B4C` | record[4] | A04_MESH_LIST_C | Overlay A04 data — a 4-record 36-byte mesh list used TWICE by branch C. Sits immediately after the last record of the 0x80143B28 sprite list. |
| `0x80143270` | record[6] | A04_TAIL_LIST_A | Overlay A04 data — 6-record mesh list, first tail prim. |
| `0x80143198` | record[6] | A04_TAIL_LIST_B | Overlay A04 data — 6-record mesh list, second tail prim. |
| `0x80105EE8` | u32 | RNG_SEED | MAIN.EXE .bss, the shared PSX rand() seed behind FUN_8009A450 / Rng::next(). Reached indirectly (once in branch B, twice in the tail). A pc_render producer MUST NOT step it — see guestWrites. |
| `0x800BF544` | u32 | PKT_POOL_CURSOR | MAIN.EXE global (`32780<<16 - 2748`), a MASKED (non-KSEG0) pool address — 0x000C4920 in the capture. Read/written only by overlay guest 0x8013AD90, which also guards on it being below (limit & 0x00FFFFFF) - 32768 where limit is 0x800D3E68 or 0x800E7E68 depending on (u8)*0x1F800135. |
| `0x800ED8C8` | u32 | OT_BASE_PTR | MAIN.EXE global; *this = the active ordering table (0x800E80A8 in the capture). overlay guest 0x8013AD90 links every point into the FIXED bucket at OT+1024. |

### Callees

| addr | role | native equivalent |
|---|---|---|
| `0x800329E0` | Entry: load the pure scene camera into GTE CR0-7 (rotation from 0x1F8000F8, translation from 0x1F80010C), set DQA(CR27) = a0 = 6, DQB(CR28) = 0. Verified from guest 0x800329E0, authenticated executable/overlay evidence. | projComposeCamera(&cam) + SpriteAnchor::baseScale(cam.H, sz, 6). Already the family contract in game/render/fx_sprite.h. codemap: leaf_800329E0 [ORPHAN] game/core/field_owned_leaves.cpp:1307. |
| `0x80032EB8` | The projection gate/publisher used by BOTH branch-A loops. RTPS the anchor already in GTE data 0/1, publish FLAG then SZ3 to 0x1F800080, apply the OT-key map, then publish SXY2 -> 0x1F80008C and MAC0 -> 0x1F800084. Returns 0 on emit, 1 on skip. 44 lines, authenticated executable/overlay evidence. | NONE. This is NOT FUN_800317CC — it is the CLAMPING twin. FUN_800317CC rejects when the mapped key leaves [4,2047] (that is what SpriteAnchor::otKeyInRange models); FUN_80032EB8 instead CLAMPS `if (k >= 2046) k = 2045` and only ever skips on (a) GTE FLAG bit 31 set or (b) SZ3 <= 0. Using otKeyInRange here would wrongly cull every far anchor. Needs a new ~10-line SpriteAnchor::otKeyClamped(sz, bias) returning false only for sz <= 0. Small pure helper, portable now. |
| `0x800328EC` | Branch-A writer wrapper: three instructions — zero 0x1F800090 then tail into FUN_8002847C(list, 0, 0). authenticated executable/overlay evidence. | Render::altSpriteEmit / the file-local emitAnimQuadRecords in game/render/fx_sprite.cpp (36-byte four-corner records, stride 36, terminal when (s32)word+4 <= 0). codemap: Render::altSpriteEmit [LIVE] game/render/fx_sprite.cpp:498. NOTE the wrapper zeroes IR0 itself, so branch A can never carry a depth cue. |
| `0x8002847C` | The 36-byte four-corner record writer reached through 0x800328EC. | emitAnimQuadRecords (file-local static in game/render/fx_sprite.cpp). codemap reports NO owner for the ADDRESS, but the behaviour is fully owned — put the new producer in fx_sprite.cpp so it can reuse the helper without exporting it. |
| `0x800318A0` | Object-transform compose for every mesh call: RotMatrix(a2 angles -> matrix at 0x1F800000) via FUN_80085480, per-column scale by the three UNSIGNED bytes at a1 each <<2 via FUN_80084520, then three MVMVA(sf=1, mx=rotation, v=IR, cv=none) column composes against the scene-camera CRs, then MVMVA(sf=1, mx=rotation, v=V0, cv=none) of the 8-byte anchor at a0 plus the camera translation, and finally upload the composed matrix to CR0-7. ~130 lines, authenticated executable/overlay evidence. | NONE as an address (codemap: no owner), but NO new port is needed: with a2 = 0x800A1CCC = {0,0,0} the base rotation is the identity, so the whole thing is `Render::projComposeObjectHost(Robj, Tobj)` with Robj = diag(f,f,f) in the 1.3.12 convention (f = byte<<2) and Tobj = the anchor's three s16. Its two leaves are already owned (Math::rotmat 0x80085480, Math::matColScale 0x80084520). |
| `0x80027768` | The game-wide packed-mesh quad writer for branches B, C and the tail. a0 = record list, a1 = clut-row bias (always 0 here), a2 = s16 OT sort bias (-160 / 0 / -100), a3 = per-frame U scroll. | FxMesh::draw (game/render/fx_mesh.cpp:165), reached through the single tap in game/render/mesh_emit_tap.cpp. BUT that path is guest-time only: it derives the transform by READING GTE CR0-7 (composedXform) and the cue by reading 0x1F800090, and it submits INTEGER screen coords with no has_xyf so it cannot lerp. A display-pass producer needs a new overload taking an explicit EObjXform + explicit ir0 and drawing through RenderQueue::drawWorldQuad. codemap also flags DUAL-OWNERSHIP of this address (fx_mesh.cpp vs swing_fx.cpp) — do not add a third install; extend FxMesh. |
| `0x8009A450` | PSX rand(). Called once in branch B (masked & 511) and twice in the tail (masked & 127) to dither the depth cue. | Rng::next() (game/core/rng.h, codemap: prng [LIVE] game/ai/beh_typed_variant_router.cpp:47). MUST NOT BE CALLED from pc_render: next() writes the shared guest seed at 0x80105EE8, which is a guest-memory write and an instant SBS divergence. |
| `0x80083F50` | rcos of (2 * angleAcc) in branch-A loop 2. | Trig::rcos (trigOf(c).rcos) — game/math/trig.h. |
| `0x80083E80` | rsin of angleAcc in branch-A loop 2. | Trig::rsin (trigOf(c).rsin) — game/math/trig.h. |
| `0x8013AD90` | Branch-B prelude: the POINT FIELD. ~218 lines (authenticated executable/overlay evidence). Reads the camera block at 0x1F8000D0, builds a base angle triple into 0x1F8000C0 from the node's own +0x2C-ish deltas via FUN_80084660/80084220/80084690, then loops (s16)*(node+0x52) + 1 times: an LCG with multiplier 0x7D2B89DD stepped 3-4 times per point produces three 11-bit angle offsets, each point is RTPS'd, and on success a THREE-WORD GP0 packet is written by hand into the pool at *(0x800BF544) — tag | 0x03000000, colour+command 0x60FFFFFF (white variable-size rectangle), XY = SXY2, WH = 0x00020002 when SZ3 < 1536 else 0x00010001 — and linked into the FIXED OT bucket at OT_BASE+1024. Guarded by a pool-space check and an on-screen X < 320 check. | NONE (codemap: no owner) and no analogue anywhere in game/render/. BIG — this is a separate producer card, not part of this port: 342 white 1x1/2x2 points, real depth from the projected SZ3, own LCG. Track it separately; a first cut of 0x8013B118 can leave it to the substrate (it will simply not appear under pc_render, exactly as today). |
| `0x80085480` | RotMatrix(SVECTOR* angles = a0, MATRIX* out = a1) inside FUN_800318A0. Note the argument order — a0 is the ANGLE vector, not the matrix. | Math::rotmat / MeshQuads::rotmat (codemap flags DUAL-OWNERSHIP). Not needed at all for this port because the angle vector is {0,0,0}. |
| `0x80084520` | Per-column 1.3.12 scale of the matrix at a0 by the three 32-bit factors at a1, inside FUN_800318A0. | Math::matColScale (game/math/gte_math.cpp:456). Folds away natively into Robj = diag(f0,f1,f2). |

### Emit contract

- **writer:** TWO writers, chosen by branch. Branch A: FUN_800328EC -> FUN_8002847C, the 36-BYTE FOUR-CORNER record writer = emitAnimQuadRecords (game/render/fx_sprite.cpp), stride 36, terminal record when (int32)word+4 <= 0 and the terminal record IS drawn. Branches B/C/TAIL: FUN_80027768, the packed-mesh quad writer = FxMesh::draw (game/render/fx_mesh.cpp), same 36-byte stride and same terminal rule, corners = s8 * 256 in model space, semi when word+4 bit 30 is set.
- **gateBias:** Branch A: 0 for BOTH loops (FUN_80032EB8(0)) — and the gate CLAMPS rather than rejects, so the only skips are SZ3 <= 0 and GTE FLAG bit 31. Branch B mesh sortBias = -160. Branch C mesh sortBias = 0. Tail mesh sortBias = -100 for both prims. The mesh sortBias is the a2 argument FxMesh::draw already turns into authoredDepthOffset(sortBias) = sortBias * 4096 / (4 * ZSF4).
- **dqa:** 6 for the whole function — FUN_800329E0(6) at entry, DQB = 0. Only branch A consumes it (as the MAC0 base scale); the mesh branches overwrite CR0-7 via FUN_800318A0 and never read MAC0.
- **scaleFormula:** Branch A loop 1 (nine fixed anchors): scaleX = scaleY = MAC0 * 3, computed as (MAC0<<1) + MAC0 — a MULTIPLY with NO shift, unlike every other member of the family. Branch A loop 2 (64 Lissajous sprites): scaleX = scaleY = MAC0 exactly, unscaled (the code merely copies 0x1F800084 into 0x1F800088). MAC0 = SpriteAnchor::baseScale(cam.H, pv.sz, 6). Both feed spriteRecordsEmit/emitAnimQuadRecords, which divide by 65536. Branches B/C/tail carry no pixel scale — their sizing is the object matrix Robj = diag(byte<<2) in 1.3.12: 320 (0.078125) for branch B from node+0x5C, 128 (0.03125) for branch C from 0x8010A48C, 640 (0.15625) for the tail from 0x8010A490.
- **ir0Formula:** Branch A: 0 — set explicitly before loop 1, and FUN_800328EC re-zeroes it on every call, so loop 2 is identity too. Branch B: IR0 = (s16)*(node+0x58) + (Rng::next() & 511), computed ONCE after the fade step and shared by all six meshes (so it uses the POST-step fade value). Branch C: IR0 = 0. Tail: recomputed PER PRIM — (s16)*(node+0x62) + (rand & 127) for 0x80143270, then (s16)*(node+0x66) + (rand & 127) for 0x80143198. Semantics with a zero far colour: out = comp * (4096 - IR0) / 4096, i.e. fx_mesh.cpp's cueScale.
- **farColour:** GTE CR21/CR22/CR23 are written to 0 on EVERY path (before branch-A loop 1, at the head of branch B's mesh section, at the head of branch C, and before the tail's FUN_800318A0). So the far colour is BLACK everywhere and every depth cue is a straight fade to black — pass farColour = {0,0,0} / use cueScale.
- **recordListFormula:** Branch A loop 1: recList = *(u32*)(0x8010A3DC + 4 * (s16)anchor[+6]), anchor from 0x8010A3EC + 8*i, i = 0..8 (modelIdx observed 0,0,0,1,1,1,2,2,2). Branch A loop 2: recList = *(u32*)(0x8010A3DC + 4 * (i & 3)), i = 0..63. Branch B: list = *(u32*)(0x8010A464 + 4*i), i = 0..5, anchor = 0x8010A434 + 8*i. Branch C: list = 0x80143B4C (constant) for both i = 0,1, anchor = 0x8010A47C + 8*i. Tail: list = 0x80143270 then 0x80143198, anchor = 0x8010A494 (single, shared transform).
- **perFrameCount:** Branch A: at most 9 + 64 = 73 anchors, one 36-byte record each (every list in the 0x8010A3DC table is a SINGLE record) -> at most 73 quads; the tail is NOT reached from this branch. Branch B: 342 GP0 white tiles from overlay guest 0x8013AD90 (out of scope for a first port) + 3+3+3+3+2+2 = 16 mesh quads, and ZERO mesh quads whenever (s16)node+0x58 >= 4096. Branch C: 2 x 4 = 8 mesh quads. Tail (branches B and C only, and only when *0x1F800207 is 2/3/4): 6 + 6 = 12 mesh quads.

### Helpers still needed

- `0x80032EB8` (~10 lines + ~6 lines of header comment): SpriteAnchor::otKeyClamped(int sz, int bias) — the CLAMPING twin of otKeyInRange. `if (sz <= 0) return false; int k = (sz>>2) + bias; if (k < 4) k = 4; k = (k >> ((k>>10)&31)) + ((k>>10) << 9); if (k >= 2046) k = 2045; return true;` (the guest's residual `k < 4 -> -1 -> skip` branch is unreachable after the pre-clamp). Declare it beside otKeyInRange in game/render/fx_sprite.h with a comment naming BOTH publishers so nobody conflates them again. The GTE-FLAG reject has no float analogue and is dropped.
- `0x80027768` (~40 lines of new code, ~20 lines moved): FxMesh::draw overload for DISPLAY-PASS use: `static void FxMesh::draw(Core*, const EObjXform&, uint32_t list, uint32_t clutRow, int32_t sortBias, uint8_t uScroll, float cue)`. Same record walk, but the transform is passed in instead of read from gte_read_ctrl (composedXform), the cue is passed in instead of read from 0x1F800090 (cueScale), and it submits through RenderQueue::drawWorldQuad with FLOAT screen XY so the panels lerp at 60 fps like the other type-0x20 producers. Refactor the existing guest-time entry point to call it after building its own EObjXform/cue, so there is one record walk. NOTE authoredDepthOffset/visibleDepth read CR30 (ZSF4) — either keep reading that CR (read-only, fine) or pass it in.
- `0x8013AD90` (~218 gen lines -> estimate 120-160 native lines): A SEPARATE producer for the 342-point white-tile field: an LCG-driven point cloud RTPS'd against the scratchpad camera block at 0x1F8000D0, drawn as 1x1/2x2 white quads (2x2 when the native sz < 1536). It has no existing analogue, and it needs its own RE pass for the 0x80084660/80084220/80084690 leaves and the base-angle setup at 0x1F8000C0. File this as its own card; do NOT block the 0x8013B118 port on it.

### Proposed wrappers

- A `namespace { }` constant block in game/render/fx_sprite.cpp, in the style already there: kA04SpriteLists = 0x8010A3DCu, kA04FixedAnchors = 0x8010A3ECu, kA04FixedCount = 9, kA04SwarmCount = 64, kA04SwarmCentreX = 19968, kA04SwarmCentreY = -6078, kA04SwarmRadius = 150, kA04LcgSeed = 5131, kA04LcgMul = 5, kA04LcgAdd = 123, kA04LcgMod = 18390, kA04ZBase = 5131, kA04AngleStep = 64, kA04FixedScaleMul = 3, kA04Dqa = 6.
- `enum class A04Mode { Sprites, MeshPanels, MeshPanelsAlt }` chosen by one named predicate each — `storyPhaseAtLeast(kA04SpriteStoryPhase /*44*/)` over G_STORY_PHASE (0x800E7EAA) and `areaProgressBit(kA04AltPanelBit /*4*/)` over G_AREA_PROGRESS (0x800BFE56) — so the top fork reads as a mode decision, not two magic `mem_r8` comparisons.
- A typed lens `struct A04Node { Core* c; uint32_t at; uint16_t uScroll() const; int16_t fade() const; uint8_t fadeDir() const; int16_t pointCount() const; uint32_t scaleCol(int i) const; int16_t tailCue(int i) const; }` so the mesh branches read `nd.fade()` / `nd.uScroll()` rather than `mem_r16(node + 0x58)`.
- A typed lens `struct SpriteAnchorRec { Core* c; uint32_t at; int16_t vx/vy/vz() const; int16_t modelIdx() const; }` over the 8-byte anchor format shared by 0x8010A3EC, 0x8010A434, 0x8010A47C and 0x8010A494 — four tables, one shape.
- A named helper `A04Lcg` with `uint32_t peek() const` and `void step()` carrying the literal `while (((uint16_t)x) >= kA04LcgMod) x -= kA04LcgMod;` reduce, with a comment stating in so many words that this is NOT a modulo because the compare is 16-bit and the subtraction is 32-bit, and that the state legitimately reaches 82863.
- `static EObjXform a04PanelXform(Render*, uint32_t anchorAddr, uint32_t scaleBytesAddr)` — the one-line native stand-in for FUN_800318A0: read three u8 column scales, build `Robj = diag(b0<<2, b1<<2, b2<<2)` in the 1.3.12 convention, read the three s16 anchor components, call projComposeObjectHost. Comment it as the FUN_800318A0 identity-angle degeneration so the next reader does not go looking for a rotation.
- Named mesh-list constants kA04MeshListsB = 0x8010A464u, kA04MeshAnchorsB = 0x8010A434u, kA04MeshAnchorsC = 0x8010A47C, kA04MeshListC = 0x80143B4Cu, kA04TailListA = 0x80143270u, kA04TailListB = 0x80143198u, kA04ScaleC = 0x8010A48Cu, kA04ScaleTail = 0x8010A490u, kA04TailAnchor = 0x8010A494u, plus kA04BiasB = -160, kA04BiasC = 0, kA04BiasTail = -100 and kA04TailPhaseLo = 2 / kA04TailPhaseCount = 3.
- A named residency guard constant `kA04RenderFnSig = 0x27BDFF28u` (addiu sp,-216) for the render_walk.cpp whitelist entry, matching the pattern the other overlay producers use.

### Risks

- THE RNG DITHER IS A REAL DESIGN FORK, not a detail. The mesh IR0 is `nodeField + (Rng::next() & 511)` (branch B) and `nodeField + (rand & 127)` (tail), and Rng::next() WRITES the guest seed at 0x80105EE8 — so a pc_render producer calling it is a guest write and an instant SBS divergence. The two honest options: (a) use only the deterministic part (node+0x58 / +0x62 / +0x66) and document the missing dither — worst case 511/4096 = 12.5% brightness on branch B, 127/4096 = 3.1% on the tail; or (b) READ the value the guest-visible behavior already left at 0x1F800090 (read-only, legal) and accept that its meaning depends on where in the frame the display pass runs — FUN_800328EC zeroes that slot and the tail overwrites it twice, so the last writer wins. Neither is free. Do NOT silently pick (a) and call it byte-faithful.
- THE AREA-4 CAPTURE DOES NOT EXERCISE MOST OF THIS FUNCTION. In scratch/raw/c18_a4.bin: story phase = 1 (so branch A is dead), progress bits = 0x0000 (so branch C is dead), node+0x58 = 4096 (so branch B's six meshes are SKIPPED), and *0x1F800207 = 1 (so the tail is SKIPPED). At that instant the ONLY thing 0x8013B118 draws is overlay guest 0x8013AD90's 342 white points — which this port does not cover. So a whitelist entry landed from this spec can legitimately produce ZERO pixels on that capture, and a null A/B is NOT evidence the port is wrong. Any acceptance gate must first drive the node into a state where node+0x58 < 4096 (set node[5] != 0 and let it ramp down) or where 0x1F800207 is 2/3/4.
- FUN_80032EB8 IS NOT FUN_800317CC. Reusing SpriteAnchor::otKeyInRange would cull every anchor whose mapped OT key reaches 2046+, which the guest instead clamps and still draws. Nine of the branch-A anchors sit at Z 4532..24114 — well into the range where this matters. Getting this backwards is a silent missing-sprites bug that looks like a distance cull.
- The recorded binary evidence decoded DATA as CODE in this overlay: 0x80143ABC, 0x80143AE0, 0x80143B04, 0x80143B28, 0x80143468, 0x801434B0, 0x801434F8 and 0x80143564 all appear in authenticated executable/overlay evidence as FUNCTIONS, but the dump proves they are 36-byte record lists (the same artifact recorded in `docs/issues/` records for 0x801346C0). Never open a guest-visible behavior at those addresses; read the dump bytes.
- FUN_800318A0's a2 = 0x800A1CCC is the ANGLE vector, not a matrix — FUN_80085480 is RotMatrix(SVECTOR* angles, MATRIX* out), a0 = angles. I read the dump and its first three s16 are 0,0,0, which is why the transform degenerates to a pure diagonal scale. If a DIFFERENT area's A04 image (or a later story state) put non-zero angles there, the mesh branches would acquire a real rotation and `a04PanelXform` would be wrong. The constant lives in MAIN.EXE .data, not the overlay, so this is unlikely — but it is an assumption resting on one dump, not on the instruction stream.
- The 342-point field's pool guard compares the MASKED pool cursor *(0x800BF544) = 0x000C4920 against (0x800D3E68 & 0x00FFFFFF) - 32768. I verified the numbers work out in the capture, but I did not RE the three leaves 0x80084660 / 0x80084220 / 0x80084690 that build the point field's base angle triple at 0x1F8000C0. Anyone porting 0x8013AD90 must do that first.
- EObjXform::project has no GTE FLAG, so the native gate drops FUN_80032EB8's `FLAG < 0 -> skip` reject. For anchors far off screen the guest skips and a native producer would draw; the drawWorldQuad path clips, so this should be invisible, but it is a real behavioural delta and not something I measured.
- Branch B's `if (fade >= 4096) skip the meshes` uses the value BEFORE the +204 step, and the IR0 it later publishes uses the value AFTER. Branch B's U-scroll is likewise stepped before the loop and the POST-step value is what all six meshes receive. Reading either at the wrong side of the mutation shifts the panels by one frame of scroll and one step of fade.
- The 64-sprite loop's VZ is stored through a 16-bit halfword store while the LCG state reaches 82863, so the truncation to uint16 is load-bearing (simulated: state 81163 -> VZ 20758). Compute in uint16 and read back signed, the way fxRingSpriteRender does. In the 64-item sequence no VZ actually goes negative, but the port must not assume that.
- The visual identity of the effect (snow / water panels / billboard swarm) is inference from geometry, scale, colour and the U-scroll idiom — not from anything I can read statically. Treat the effectGuess as a hypothesis to confirm with a screenshot, not as spec.


---

## 0x80116904 — area 8 (A08 overlay)

**Portable:** moderate — All the maths is integer and fully resolved; the projection folds cleanly onto projComposeCamera (the guest's SetRotMatrix(camera)/ApplyRotMatrix(offs)/SetTransMatrix(camT+R*offs) is exactly cam.T[i] += clamp16((R[i].offs)>>12)), and a native LINE producer already exists in game/render/fx_line.cpp (strokeSegment / worldLineDraw) so the emit side is solved. What makes it MODERATE rather than straightforward is STATE: the effect is inherently one-frame-differential and the guest keeps its previous-frame state in guest memory (the 32-entry screen-position array at 0x801485E8 and the previous cube base at node+0x48/0x4A/0x4C). pc_render must not write guest memory, and reading the guest's copies is order-dependent on when the substrate's own render walk ran, so the producer has to carry its OWN host-side shadow of the previous frame's projected positions + validity, updated on real frames only. That is the same 'one frame behind' pattern fps60 already uses, but it is new state for this file. No callee is a blocker: the two unowned callees (SetRotMatrix 0x80084660, SetTransMatrix 0x80084690) are 5-line GTE control-register loads that a native producer does not need at all.

**Effect:** 

**Summary:** overlay guest 0x80116904 (overlay A08, area 8) is NOT a member of the FUN_80027A4C/FUN_8002847C sprite-record family — it is a self-contained motion-streak emitter that builds GPU primitives itself. It draws 32 world-space "motes" as gouraud semi-transparent 2-point LINES (GP0 code 0x52) from each mote's CURRENT screen position back to an extrapolated point 2x beyond its PREVIOUS-frame screen position, i.e. a doubled-length motion streak, white (0xFFFFFF) at the head and mid-grey (0x808080) at the tail. Mote positions come from a deterministic LCG seeded from node+0x50 (never written back, so the pattern is stable frame to frame), stepped THREE times per mote, masked to 11 bits so the field lives in a 2048-unit cube that is re-centred on the camera every frame (centre = camera world position - 1024 + half the camera's forward row). Because the cube is camera-locked but each mote's world coordinate is (lcg + nodeAnchor), the motes hold WORLD positions and wrap; a per-axis bit-11 XOR test against the previous frame's cube base suppresses the streak exactly on the frame a mote wraps, so no line ever streaks across the volume. The sibling behaviour fn 0x8011B398 advances the shared node anchor by (-12, +101, +56) every frame and Y is DOWN in this engine, so this is falling, slightly wind-blown RAIN (or fast-falling debris) — 101 units/frame of drop plus camera parallax is what makes the streak. Instead of the family's (SZ3>>2)+bias gate it computes the same logarithmic OT key with bias 0 and NO pre-clamp, range-gates it to [4,2047], and links a LINE_G2 (20 B) plus a DR_MODE (12 B, dither on, tpage 21 -> blend mode 0 = B/2+F/2) into that OT bucket from the shared packet pool at *0x800BF544. Per-mote streak history (32 x {s16 x, s16 y}) lives in a single overlay-resident array at 0x801485E8 with 0x7FFF as the "no previous point" sentinel, and the previous cube base is stashed in node+0x48/0x4A/0x4C.


### Verifier corrections (CORRECTED)

- **[MAJOR]** claim: nodeFields +0x04 / globals 0x801485E8: state 0 = init 'fills the prev array with the 0x7FFF7FFF sentinel'; and 'Also filled with 0x7FFF7FFF wholesale by the sibling behaviour 0x8011B398 on init and whenever the effect is suppressed.'

  actual: FALSE. Both fill loops in overlay guest 0x8011B398 store to a FIXED address that is never advanced. At L_8011B3EC (authenticated executable/overlay evidence) the loop is: r3+=1; r2=(r3<32); mem_w32(r6+0)=0x7FFF7FFF (delay slot); branch. r6 = 0x80150000-31256 = 0x801485E8 is set ONCE before the loop and there is no addiu. Identical at L_8011B448 (line 8963-8965) with r5. So 32 iterations write the SAME word — only mote 0's history entry is invalidated; entries 1..31 keep whatever they held. (The recorded binary evidence does emit pointer increments where they exist — the render fn's own loop tail has `c->r[20] = c->r[20] + 4` — so this is faithful, i.e. a guest bug, not a transliteration artifact.) This falsifies the risks[] reasoning about 'the guest resets its history to the sentinel' on resume.

- **[MINOR]** claim: emitContract.gateBias: 'There are two gates in front of the key gate: RTPS FLAG (CR31) sign bit must be clear, and the per-axis bit-11 wrap test must pass on all three axes.'

  actual: Wrong order, and it contradicts the spec's own algorithm steps 11/12. The instruction stream at L_80116B7C loads the OT key from 0x1F800080 and branches to the fail path FIRST (`if ((int32_t)r2 < 0) goto L_80116BD0`); only after that does it compute the three XOR/&0x800 wrap tests. Order is FLAG -> OT key -> wrap -> history sentinel. Behaviourally equivalent (key-fail and wrap-fail land on the same sentinel write), but the spec states it as fact in the contract section.

- **[MINOR]** claim: algorithm step 10: 'PUBLISH: *(u32*)(pool+8) = gte_read_data(14) ... this happens even on the paths that emit nothing, so pool+8/pool+10 always hold the current screen X/Y.'

  actual: Not on the FLAG-fail path. The store is the first instruction of L_80116B40, which is reached only when `(int32_t)FLAG >= 0`. On FLAG failure the code jumps straight to the sentinel write and pool+8/pool+10 still hold whatever the PREVIOUS mote left there. Any port (or any reader reasoning about the guest's scratch) must not assume pool+8 is current after an RTPS overflow.

- **[MINOR]** claim: summary: 'masked to 11 bits so the field lives in a 2048-unit cube that is re-centred on the camera every frame (centre = camera world position - 1024 + half the camera's forward row)'

  actual: That expression is the cube's MINIMUM CORNER (`offs`), not its centre. The GTE evaluates camR·(V + offs) + camT with V = (g+base) & 0x7FF in [0,2047], so the cube spans [offs, offs+2047] and the CENTRE is campos + halfForwardRow (the -1024 and the +1023.5 half-span cancel). The globals[] note ('Halved to bias the wrap cube ~2048 units in front of the camera') is the correct reading; the summary contradicts it.

- **[MINOR]** claim: proposedWrappers: 'struct WrapCube { int32_t cx, cy, cz; } + rainWrapCube(...) — computes the camera-locked cube centre: T2_CAM_POS_* (unsigned high halfword) - kCubeHalfSpan + halfForward(cam.R[2][i])'

  actual: Drops the s16 truncation that the spec's own step 3 identifies, and that truncation is not academic: the gen stores each component with mem_w16 to sp+24/26/28 and every subsequent consumer reads it back sign-extended (`r2<<16; (int32_t)r2>>16`) — and the GTE sees the SVECTOR halfwords too. camPos is read UNSIGNED (0..65535), so camPos-1024+h routinely exceeds s16 and wraps. An int32_t WrapCube diverges from the guest for any camera X/Y/Z whose high halfword is >= 0x8000+1024-h. The wrapper must be s16 (or explicitly truncate).

- **[MINOR]** claim: portable.reason: 'a native LINE producer already exists in game/render/fx_line.cpp (strokeSegment / worldLineDraw) so the emit side is solved.'

  actual: Overstated. strokeSegment (game/render/fx_line.cpp:104) is a file-local inline inside an anonymous namespace, takes ONE `unsigned char grey` applied to all four vertices, and hardcodes `kRopeBlend = 3` (B + F/4). The rain streak needs a two-colour gouraud gradient (0xFFFFFF head -> 0x808080 tail) at blend mode 0 (B/2 + F/2). That is a new variant with a new signature, not a reuse — the emit side is a template, not solved.

- **[MINOR]** claim: globals 0x800BF816: 'kPauseFreezeFlag — MAIN.EXE global (written by game/world/pool.cpp). Non-zero -> ... so nothing streaks across a pause.'

  actual: The POLARITY is right (0x8011B398 suppresses when the byte is non-zero), but the name/semantics are invented and collide with an existing one: this codebase already names 0x800BF816 `kUiBusy` — 'u8: UI-busy latch (dialog/menu up)' (game/render/field_hud.cpp:44), and pool.cpp:231 writes 1 to it. Elsewhere (game/ai/beh_visibility_gate_dispatch.cpp:59, beh_anim_trigger_gates.cpp:117) a ZERO value means 'skip', i.e. the opposite sense of 'freeze'. Adopting kPauseFreezeFlag would fork the naming on an unverified reading.

- **[MINOR]** claim: globals 0x800ED8C8: 'MAIN.EXE global (already named kOtBasePtrPtr / OTBASE_PTR).'

  actual: `kOtBasePtrPtr` does not exist anywhere in game/. The existing names are `kOtBaseGlobal` (game/render/tile_grid_layer.cpp:156), `OT_BASE_GLOBAL` (game/render/overlay_ground_gt3gt4.cpp:353) and `OTBASE_PTR` (game/render/perobj_billboard.cpp:100).

- **[MINOR]** claim: algorithm step 20 (native port recipe) / emitContract.dqa 'NONE': the recipe projects with projComposeCamera and takes ofx/ofy/H from it.

  actual: Incomplete rather than wrong. The function loads only CR0-4 (SetRotMatrix) and CR5-7 (SetTransMatrix) — it never writes CR24/CR25/CR26 (OFX/OFY/H) nor CR27/CR28 (DQA/DQB). Its RTPS therefore INHERITS the projection constants whatever the previous GTE user left them at. Substituting projComposeCamera's ofx/ofy/H is almost certainly right (the field renderer sets them per frame from the same scene camera), but the spec asserts the projection is self-contained and it is not; the inheritance should be stated as an assumption.


### Algorithm (as CORRECTED by the verifier — a DELTA; read with the ORIGINAL below)

1. PROLOGUE. sp -= 104. r21 = 0x1F800080 (FLAG / SZ3 / OT-key scratch slot). r20 = 0x801485E8 (prev[i].x cursor), r22 = r20+2 (prev[i].y) — one shared interleaved {s16 x, s16 y}[32], stride 4, both advanced by 4 per mote. r17 = 0x1F8000D0 (camera scratch block). r18 = *(u32*)0x800BF544 (packet-pool bump cursor). sp+36 = 32 (loop counter). sp+56 = 0x7FFF7FFF (the sentinel word).
2. HALF-FORWARD ROW. h0/h1/h2 = floor(v/2) of (s16)*0x1F800104 / 0x1F800106 / 0x1F800108, i.e. m[2][0..2] of the scene-camera MATRIX at 0x1F8000F8 (its third rotation ROW = the camera forward axis in world coords). The guest spells floor(v/2) as (v<<11)>>12; that idiom must NOT survive into the port.
3. WRAP-CUBE MIN CORNER `offs` (SVECTOR at sp+24/26/28). offs.x = (u16)*0x1F8000D2 - 1024 + h0; offs.y = (u16)*0x1F8000D6 - 1024 + h1; offs.z = (u16)*0x1F8000DA - 1024 + h2. Camera reads are UNSIGNED; each component is TRUNCATED TO s16 by the store and read back SIGN-EXTENDED (this wraps in practice — do not model it as int32). `offs` is the cube's MINIMUM CORNER: world = V + offs with V in [0,2047], so the cube CENTRE is campos + halfForwardRow.
4. PER-FRAME CUBE BASE. baseX/Y/Z = (s16)node+0x2C/0x2E/0x30 - (s16)offs.x/y/z, kept full 32-bit. Snapshot prevBaseX/Y/Z = (s16)node+0x48/0x4A/0x4C.
5. GTE TRANSFORM. SetRotMatrix(0x1F8000F8) -> CR0-4. ApplyMatrixLV(&offs, 0x1F800014) = MVMVA sf=1 mx=ROT v=V0 cv=none lm=0 -> t[i] = clamp_s16((camR·offs)>>12). t[i] += *(0x1F80010C + 4i) (the scene camera translation). SetTransMatrix(0x1F800000) -> CR5-7. NOTE: OFX/OFY/H (CR24-26) and DQA/DQB (CR27-28) are NEVER written here — the RTPS inherits them from whatever ran last.
6. LOOP, 32 iterations. K = *(u32*)0x801450D8 (re-read each iteration; constant overlay data). Signed multiply-low, seed = seed*K + 1, stepped THREE times per mote. gx = (s32)s0 >> 16 (captured BEFORE the first step), gy = (s32)s1 >> 16, gz = (s32)s2 >> 16; s3 carries to the next mote.
7. STAGE + PROJECT. ax/ay/az = g + base (full 32-bit, kept unmasked for the wrap test). Write (a & 2047) as u16 to 0x1F8000C0/C2/C4; VXY0 = *(u32*)0x1F8000C0, VZ0 = *(u32*)0x1F8000C4. RTPS (gte_op 0x4A180001, sf=1, lm=0).
8. GATE 1 — FLAG. *0x1F800080 = gte_read_ctrl(31); if (s32)FLAG < 0 -> write 0x7FFF7FFF to prev[i] (one 32-bit store covering both halves) and go to the next mote. NOTHING is published on this path — pool+8/pool+10 still hold the PREVIOUS mote's screen point.
9. PUBLISH (only past gate 1). *(u32*)(pool+8) = gte_read_data(14) (SXY2). *0x1F800080 = gte_read_data(19) (SZ3).
10. GATE 2 — OT KEY (evaluated BEFORE the wrap test). e = SZ3 >> 12; key = ((SZ3 >> 2) >> (e & 31)) + (e << 9); keep iff (u32)(key - 4) < 2044 (key in [4,2047]) else key = -1; if key < 0 -> sentinel, next mote. Bias 0 and NO pre-clamp: this REJECTS SZ3 in [0,15], unlike SpriteAnchor::otKeyInRange which clamps (sz>>2)+bias to >= 4 first and would ACCEPT it. Port as a separate otKeyRaw, do not reuse otKeyInRange.
11. GATE 3 — WRAP (per axis, bit 0x800). Fail if ((a_axis) ^ (g_axis + prevBase_axis)) & 2048 on X, Y or Z -> sentinel, next mote.
12. GATE 4 — HISTORY. If (s16)prev[i].x == 32767: store the current SXY2 halves into prev[i] and continue — no packet, pool cursor NOT advanced.
13. EMIT (all four gates passed). LINE_G2 at pool, 20 B: pool+4 = 0x52FFFFFF (GP0 0x52 gouraud semi-transparent 2-point line, colour0 white), pool+8 = current SXY2, pool+12 = 0x00808080 (colour1 mid grey), pool+16/+18 = tail. Tail order: dx = (s16)prev.x - (s16)cur.x; dy = (s16)prev.y - (s16)cur.y; dx<<=1; dy<<=1; tail = (u16)cur + d, stored u16 — algebraically 2*prev - cur with u16 wrap.
14. LINK. otSlot = *(u32*)0x800ED8C8 + key*4; *(u32*)pool = *otSlot | 0x04000000; *otSlot = pool. Store prev[i].x/y = current SXY2 halves. pool += 20.
15. DR_MODE HEADER. SetDrawMode(pool, dfe=0, dtd=1, tpage=21, tw=NULL) -> pool+3 = 2, pool+4 = 0xE1000215, pool+8 = 0. Re-link into the SAME bucket with | 0x02000000; pool += 12. Head-insert ordering puts the DR_MODE ahead of the line. tpage 0x15 = base (320,256), semi bits 0 = B/2 + F/2, bit 9 = dither on.
16. EPILOGUE. *0x800BF544 = pool. node+0x48 = (u16)baseX, node+0x4A = low u16 of baseY, node+0x4C = (u16)baseZ. sp += 104.
17. SIBLING BEHAVIOUR 0x8011B398 (not ported; substrate keeps running it) — CORRECTED. state@node+4: 0 -> writes the sentinel to 0x801485E8 THIRTY-TWO TIMES AT THE SAME ADDRESS (the loop pointer is never advanced — ONLY mote 0's history is invalidated), sets state = 1 and falls through; 1 -> run; 2 or 3 -> despawn (0x8007A624); >= 4 -> return. Suppression gates: (u16)*0x800BFE56 & 0x100 -> node+1 = 0, return; (u8)*0x800BF816 (already named kUiBusy in game/render/field_hud.cpp) non-zero -> node+1 = 0 plus the same one-entry 'fill'. Normal: node+1 = 1, node+0x2C -= 12, node+0x2E += 101, node+0x30 += 56.
18. NATIVE PORT RECIPE. EObjXform cam; projComposeCamera(&cam) (R is raw 1.3.12; T raw view units; ofx/ofy/H inherited-by-assumption from the same scene camera). offs as s16 components from T2_CAM_POS_X/Y/Z (unsigned) - 1024 + floor(cam.R[2][i]/2), TRUNCATED TO s16. Fold the guest's SetTransMatrix into the camera: cam.T[i] += clamp_s16(floor((cam.R[i][0]*offs.x + cam.R[i][1]*offs.y + cam.R[i][2]*offs.z)/4096)). Per mote: cam.project(vx,vy,vz,&pv) on the masked 0..2047 triple; gate on the NO-PRE-CLAMP key formula; gate on the bit-0x800 wrap test on all three axes; then draw a 1px screen-space stroke from (pv.px,pv.py) grey 255 to (2*prevPx - pv.px, 2*prevPy - pv.py) grey 128 with a GOURAUD two-colour variant of fx_line.cpp's strokeSegment at blend mode 0 (strokeSegment itself is monochrome and hardcodes kRopeBlend = 3 — a new variant is required), depth proj_pz_to_ord(pv.pz) on both ends, semi = 1. Keep 32 previous screen points + per-mote validity in HOST state on Render, refreshed on real frames only; note in-code that the guest's own reset only clears entry 0, so a full host-side invalidation on resume is a DELIBERATE divergence from the guest bug.

### Algorithm (ORIGINAL spec steps — the corrections above are stated against these)

1. PROLOGUE / fixed pointers. sp -= 104. r21 = 0x1F800080 (OT-key + FLAG scratch slot). r20 = 0x801485E8 (prevX cursor), r22 = r20+2 = 0x801485EA (prevY cursor) — one interleaved {s16 x, s16 y} array of 32 entries, stride 4, walked with r20/r22 both += 4 per mote. r17 = 0x1F8000D0 (camera scratch block). r18 = *(u32*)0x800BF544 = packet-pool bump cursor. sp+36 = loop counter = 32. sp+56 = 0x7FFF7FFF (the 'no previous point' sentinel word, written to prev[i] on every skip).
2. READ THE CAMERA FORWARD ROW. h0 = (s16)*(0x1F800104) = camR.m[2][0]; h1 = (s16)*(0x1F800106) = m[2][1]; h2 = (s16)*(0x1F800108) = m[2][2]. Each is transformed as (v << 11) >> 12 arithmetic, i.e. EXACTLY floor(v/2) (v is s16 so the <<11 never overflows s32). Call these h0/h1/h2 = the half-forward row.
3. BUILD THE WRAP-CUBE CENTRE OFFSET `offs` (an SVECTOR at sp+24/26/28, s16 slots, all arithmetic done in 16-bit and allowed to wrap). offs.x = (u16)*(0x1F8000D2) - 1024 + h0;  offs.y = (u16)*(0x1F8000D6) - 1024 + h1;  offs.z = (u16)*(0x1F8000DA) - 1024 + h2. The three camera reads are the HIGH halfwords of the 16.16 camera world position (T2_CAM_POS_X/Y/Z), taken UNSIGNED. Each result is stored to a 16-bit slot and later read back sign-extended. (The gen stores each slot twice — the first store, before the h term is added, is dead.)
4. BUILD THE PER-FRAME CUBE BASE from the node's own animated world anchor. baseX (r30) = (s16)node+0x2C - (s16)offs.x;  baseY (sp+40) = (s16)node+0x2E - (s16)offs.y;  baseZ (r23) = (s16)node+0x30 - (s16)offs.z. Also snapshot the PREVIOUS frame's bases: prevBaseX (sp+44) = (s16)node+0x48, prevBaseY (sp+48) = (s16)node+0x4A, prevBaseZ (sp+52) = (s16)node+0x4C. NOTE sp+40 is reused: it briefly holds h1 before being overwritten with baseY.
5. COMPOSE THE GTE TRANSFORM. (a) SetRotMatrix(0x1F8000F8) [FUN_80084660] loads the scene camera rotation into GTE CR0-4. (b) ApplyMatrixLV(&offs, 0x1F800014) [FUN_80084220 = Math::applyMatlv] computes t = clamp_s16((camR . offs) >> 12) into the three longs at 0x1F800014/18/1C (which are the t[] slots of a MATRIX based at 0x1F800000). (c) t[i] += *(0x1F80010C + 4i) — the scene camera's own translation. (d) SetTransMatrix(0x1F800000) [FUN_80084690] loads CR5-7 = t. Net effect: for a mote vector V the GTE computes camR . (V + offs) + camT, i.e. `offs` is the world-space centre of the wrap cube and V is the mote's position inside it.
6. LOOP: 32 iterations (counter at sp+36, decremented at the bottom, `while (--n > 0)`). Per iteration r20/r22 advance by 4 and the LCG (r19, initialised ONCE before the loop from *(u32*)(node+0x50)) is stepped exactly THREE times. K = *(u32*)0x801450D8 (overlay-resident LCG multiplier). Step is seed = seed*K + 1 with a SIGNED 32x32 multiply, low word kept.
7. PER-MOTE COORDINATE DERIVATION — the read points relative to the mutation matter. Let s0 be the seed on entry to the iteration. (1) compute lo = s0*K; capture gx = (s32)s0 >> 16 (the OLD seed's high half — read BEFORE the seed is replaced); s1 = lo+1. (2) compute lo = s1*K; capture gy = (s32)s1 >> 16; s2 = lo+1. (3) capture gz = (s32)s2 >> 16. (4) compute lo = s2*K; s3 = lo+1 — s3 is the seed carried into the NEXT iteration. So the axes read the chain at s0 / s1 / s2 and the chain advances 3 times per mote.
8. SVECTOR STAGING. ax = gx + baseX, ay = gy + baseY, az = gz + baseZ (all full 32-bit sums, kept unmasked for the wrap test). The GTE input is vx = ax & 2047, vy = ay & 2047, vz = az & 2047, written as three u16 to 0x1F8000C0/C2/C4, then loaded as VXY0 = *(u32*)0x1F8000C0 and VZ0 = *(u32*)0x1F8000C4 (the upper half of that second load is stale scratch and is ignored by the GTE's 16-bit VZ0).
9. RTPS: gte_op(0x4A180001) = RTPS, sf=1 (>>12), lm=0. Then FLAG = gte_read_ctrl(31) is stored to 0x1F800080; if (s32)FLAG < 0 -> FAIL (write 0x7FFF7FFF to prev[i], no primitive, next mote).
10. PUBLISH: *(u32*)(pool+8) = gte_read_data(14) (SXY2, the packed screen point) — this happens even on the paths that emit nothing, so pool+8/pool+10 always hold the current screen X/Y. *(u32*)0x1F800080 = gte_read_data(19) (SZ3).
11. OT-KEY GATE (the family's logarithmic bucket map, bias 0, NO pre-clamp): let z = SZ3 (u16 0..65535); e = z >> 12; key = ((z >> 2) >> (e & 31)) + (e << 9). If (u32)(key - 4) < 2044 keep key, else key = -1. Then if (s32)key < 0 -> FAIL (sentinel, next mote). Valid keys are exactly [4, 2047]. NOTE: SpriteAnchor::otKeyInRange(sz, 0) is the SAME formula but clamps k to >= 4 BEFORE the log map, so it accepts sz in [0,15] where this emitter rejects it — do not reuse it verbatim.
12. WRAP GATE (per axis, bit 11): fail if ((ax ^ (gx + prevBaseX)) & 2048) != 0, or ((ay ^ (gy + prevBaseY)) & 2048) != 0, or ((az ^ (gz + prevBaseZ)) & 2048) != 0. This asks 'did this mote's 11-bit wrapped coordinate roll over since last frame'; if so -> FAIL (sentinel, next mote), so a wrapped mote draws no streak.
13. HISTORY GATE: if (s16)prev[i].x == 32767 (the sentinel) -> no primitive; just store the current screen X/Y into prev[i] and go to the next mote (pool cursor NOT advanced).
14. EMIT (all four gates passed). Packet at `pool` is a LINE_G2, 20 bytes: pool+4 = 0x52FFFFFF (GP0 0x52 = gouraud line, semi-transparent, 2-point; colour0 = white 0xFFFFFF); pool+8 = current SXY2 (already written); pool+12 = 0x00808080 (colour1 = mid grey); pool+16/18 = the tail point.
15. TAIL POINT (exact order): dx = (s16)prev[i].x - (s16)pool[+8]; dy = (s16)prev[i].y - (s16)pool[+10]; dx <<= 1; dy <<= 1; tailX = (u16)pool[+8] + dx; tailY = (u16)pool[+10] + dy; stored as u16 at pool+16/+18. Algebraically tail = 2*prev - cur, i.e. the streak runs from the current head to a point one full frame-delta BEYOND the previous position — the streak is twice the real per-frame motion.
16. LINK THE LINE: otSlot = *(u32*)0x800ED8C8 + key*4; *(u32*)pool = *(u32*)otSlot | 0x04000000 (tag: old head in the low 24 bits, length nibble 4 = the four words after the tag); *(u32*)otSlot = pool.
17. STORE HISTORY: prev[i].x = (u16)pool[+8]; prev[i].y = (u16)pool[+10]. Then pool += 20.
18. EMIT THE DR_MODE HEADER: SetDrawMode [FUN_80083DE0] (a0 = pool, a1 = 0 (dfe), a2 = 1 (dtd/dither), a3 = 21 (tpage), 5th arg at sp+16 = 0 (no texture window)) -> pool+3 = 2, pool+4 = 0xE1000215, pool+8 = 0. Then otSlot = *(u32*)0x800ED8C8 + key*4 (recomputed); *(u32*)pool = *(u32*)otSlot | 0x02000000 (length 2); *(u32*)otSlot = pool; pool += 12. Because both packets go to the SAME bucket and the OT is a head-insert list, the DR_MODE executes BEFORE the line. tpage 21 = 0x15 -> tpage base (320,256), semi-transparency mode bits (5-6) = 0 -> blend 0.5*B + 0.5*F; bit 9 set = dither on.
19. EPILOGUE: *(u32*)0x800BF544 = pool (write the bump cursor back). node+0x48 = (u16)baseX, node+0x4A = (u16)baseY (taken from the LOW halfword of sp+40), node+0x4C = (u16)baseZ — this frame's cube base becomes next frame's prevBase. Restore r16-r23/r30/r31, sp += 104.
20. NATIVE PORT RECIPE (read-only). EObjXform cam; projComposeCamera(&cam). Compute offs from cam.R[2][0..2] (= the same guest m[2][*]) with floor(v/2), and from T2_CAM_POS_X/Y/Z. Fold the guest's translation composition into the camera: for i in 0..2, d[i] = clamp_s16( floor((cam.R[i][0]*offs.x + cam.R[i][1]*offs.y + cam.R[i][2]*offs.z) / 4096) ); cam.T[i] += (float)d[i]. Then for each mote just cam.project(vx, vy, vz, &pv) with the masked 0..2047 triple. Gate on pv.sz via the no-pre-clamp key formula, gate on the bit-11 wrap test, then draw the streak with the fx_line.cpp strokeSegment shape: head (pv.px, pv.py) colour 255, tail (2*prevPx - pv.px, 2*prevPy - pv.py) colour 128, depth proj_pz_to_ord(pv.pz) on both ends, semi=1, blend mode 0, 1px stroke. Keep the 32 previous screen points + a per-mote valid flag in HOST state on Render, refreshed only on real (non-interp) frames.

### Node fields

| offset | type | name | meaning |
|---|---|---|---|
| `+0x01` | u8 | visible | per-frame visibility marker; render_walk skips the node when 0. Set to 0 by the sibling behaviour 0x8011B398 when the effect is suppressed. |
| `+0x04` | u8 | state | behaviour state (0x8011B398): 0 = init (fills the prev array with the 0x7FFF7FFF sentinel, then -> 1), 1 = running, 2/3 -> despawn (0x8007A624). |
| `+0x0B` | u8 | nodeType | 0x20 = custom-render-fn node (the render_walk type gate). |
| `+0x18` | u32 | renderFn | = 0x80116904 for this effect; the value the render_walk whitelist keys on. |
| `+0x2C` | s16 | anchorX | world anchor X. Advanced by -12 every frame by behaviour 0x8011B398. |
| `+0x2E` | s16 | anchorY | world anchor Y. Advanced by +101 every frame (Y is DOWN in this engine) — this is the fall speed. |
| `+0x30` | s16 | anchorZ | world anchor Z. Advanced by +56 every frame — the sideways drift. |
| `+0x48` | s16 | prevBaseX | previous frame's cube base X (written by THIS render fn at its epilogue). Input to the bit-11 wrap test. |
| `+0x4A` | s16 | prevBaseY | previous frame's cube base Y. |
| `+0x4C` | s16 | prevBaseZ | previous frame's cube base Z. |
| `+0x50` | u32 | lcgSeed | LCG seed for the mote lattice. READ ONCE per frame and NEVER written back, so the 32 motes occupy the same lattice offsets every frame — all motion comes from the anchor and the camera. |

### Globals

| addr | type | name | meaning |
|---|---|---|---|
| `0x1F800000` | MATRIX (only t[] at +20/+24/+28 used) | kComposedTransMat | SCRATCHPAD scratch matrix. ApplyMatrixLV writes camR.offs>>12 into its t[] (0x1F800014/18/1C), the code adds the camera translation there, SetTransMatrix loads CR5-7 from it. Guest scratch only — the native port folds this into cam.T. |
| `0x1F800080` | s32 | kOtKeyScratch | SCRATCHPAD. Reused twice per mote: first as the landing slot for the RTPS FLAG (CR31) sign test, then for SZ3 and finally the computed OT bucket key (or -1). Same slot the sprite family publishes its OT key to. |
| `0x1F8000C0` | s16[3] (VX,VY,VZ) | kMoteSvec | SCRATCHPAD staging for the mote's masked coordinate triple; loaded into GTE VXY0/VZ0 as two 32-bit reads at +0 and +4 (the upper half of the second read is stale and ignored). |
| `0x1F8000D2` | u16 | T2_CAM_POS_X | SCRATCHPAD camera block (0x1F8000D0). High halfword of the 16.16 camera world X; read UNSIGNED. Already named in game/tomba2_types.h. |
| `0x1F8000D6` | u16 | T2_CAM_POS_Y | High halfword of the 16.16 camera world Y (game/tomba2_types.h). NOTE game/render/cull.cpp's local comments call +6 'Z' and +10 'Y' — the header's X/Y/Z naming is the one that matches the SVECTOR order used here. |
| `0x1F8000DA` | u16 | T2_CAM_POS_Z | High halfword of the 16.16 camera world Z. |
| `0x1F8000F8` | MATRIX | kSceneCamMatrix | SCRATCHPAD scene camera view matrix — m[3][3] at +0..+17, t[] at +20/24/28. This is exactly what Fps60::sceneCam / projComposeCamera read. |
| `0x1F800104 / 0x1F800106 / 0x1F800108` | s16 x3 | kSceneCamFwdRow | = kSceneCamMatrix m[2][0..2], the camera's third rotation ROW (its forward axis in world coords). Halved to bias the wrap cube ~2048 units in front of the camera. Natively = cam.R[2][0..2]. |
| `0x1F80010C / 0x1F800110 / 0x1F800114` | s32 x3 | kSceneCamTrans | = kSceneCamMatrix t[0..2], the camera view translation. Natively = cam.T[0..2]. |
| `0x801450D8` | u32 | kMoteLcgMult | A08-OVERLAY DATA. The LCG multiplier (seed = seed*mult + 1). Only read here. Valid only while overlay A08 is resident — read it live, never bake it. |
| `0x801485E8` | struct { s16 x; s16 y; } [32] | kMoteStreakHistory | A08-OVERLAY DATA, 128 bytes. Previous frame's projected screen point per mote; x == 32767 (0x7FFF) is the 'no previous point / invalidated' sentinel. Also filled with 0x7FFF7FFF wholesale by the sibling behaviour 0x8011B398 on init and whenever the effect is suppressed. It is a SINGLE SHARED buffer, not per-node. |
| `0x800BF544` | u32 | kPktPoolCursor | MAIN.EXE global (already named kPktPoolPtr / PKT_POOL_PTR across game/render/). Packet-pool bump-allocator cursor; read at entry, written back at exit. |
| `0x800ED8C8` | u32 | kOtBasePtr | MAIN.EXE global (already named kOtBasePtrPtr / OTBASE_PTR). Holds the live ordering-table base; the bucket slot is *this + key*4. |
| `0x800BFE56` | u16 | kAreaCollectedBits | MAIN.EXE global, per-area collected bitmask (game/world/placement.cpp). Behaviour 0x8011B398 clears node+1 (invisible) when bit 0x100 (= area 8) is set, so the effect is quest-gated. |
| `0x800BF816` | u8 | kPauseFreezeFlag | MAIN.EXE global (written by game/world/pool.cpp). Non-zero -> behaviour 0x8011B398 clears node+1 AND re-fills the whole streak history with the sentinel, so nothing streaks across a pause. |

### Callees

| addr | role | native equivalent |
|---|---|---|
| `0x80084660` | libgte SetRotMatrix(MATRIX* m at 0x1F8000F8) — loads m->m[3][3] into GTE CR0-4. | NONE (codemap: no native owner). Not needed by the port: projComposeCamera already supplies the same rotation as EObjXform::R. If a native owner is ever wanted it is a 12-line guest-visible behavior (5 loads + 5 gte_write_ctrl), trivially portable. |
| `0x80084220` | libgte ApplyMatrixLV(SVECTOR* offs, VECTOR* out at 0x1F800014) — MVMVA sf=1 mx=ROT v=V0 cv=Null lm=0 using CR0-4; writes clamp_s16((R.offs)>>12) as three sign-extended longs. | Math::applyMatlv (game/math/gte_math.cpp:351) — LIVE. The port does not call it: fold the result into cam.T as d[i] = clamp_s16(floor((R[i].offs)/4096)). |
| `0x80084690` | libgte SetTransMatrix(MATRIX* m at 0x1F800000) — loads m->t[0..2] (bytes +20/+24/+28) into GTE CR5-7. | NONE (codemap: no native owner). Not needed by the port. 9-line guest-visible behavior (3 loads + 3 gte_write_ctrl), trivially portable if ever wanted. |
| `0x80083DE0` | libgpu SetDrawMode(DR_MODE* p, dfe=0, dtd=1, tpage=21, tw=NULL) — builds the 12-byte draw-mode header linked ahead of each line. | guest 0x80083DE0 exists in game/render/wide_re_libgpu_leaves.cpp:280 but it is a STATIC, UNWIRED draft AND IT IS WRONG: the guest-visible behavior takes the 0x9FF tpage bits from a3 (c->r[7]) and uses a1 (c->r[5]) only as the zero-test that ORs 0x400, while the draft takes both from a1 and never reads a3. That is the standard libgpu signature (p, dfe, dtd, tpage, tw) — the draft has dfe and tpage swapped. The port needs no equivalent: the output word here is the constant 0xE1000215 (dither on, tpage 21, semi-transparency mode 0 -> B/2 + F/2), which the native producer expresses as blend mode 0 on the queue emit. |
| `0x8011B398` | NOT called by the render fn — the sibling BEHAVIOUR for the same node. Owns the state byte at node+4, the visibility byte at node+1, the sentinel fill of the streak history, and the per-frame anchor advance (-12, +101, +56). | NONE (codemap: no native owner). Read-only relevance: it is where the 'is this effect rain' evidence and the suppression gates live. A read-only producer must NOT port it — it mutates guest state and the substrate still runs it. |

### Emit contract

- **writer:** NEITHER family writer. This member builds its primitives inline: one LINE_G2 (GP0 code 0x52 — gouraud, semi-transparent, 2-point; 5 words / 20 bytes: tag, 0x52FFFFFF, xy0, 0x00808080, xy1) plus one DR_MODE (3 words / 12 bytes: tag, 0xE1000215, 0x00000000) per drawn mote, both bump-allocated from *0x800BF544 and head-inserted into the SAME OT bucket. FUN_80027A4C / FUN_8002847C are never reached, so Render::spriteRecordsEmit and emitAnimQuadRecords are NOT the native emit path — game/render/fx_line.cpp's strokeSegment (screen-space 1px stroke -> quad, RQ_WORLD / RQ_OM_DEPTH) is.
- **gateBias:** 0 (and there is no near/far bias field on the node). The key is computed straight from SZ3 with no additive bias, and — unlike SpriteAnchor::otKeyInRange — there is NO clamp of the key to >= 4 before the logarithmic map, so SZ3 in [0,15] is REJECTED here whereas otKeyInRange would accept it. There are two gates in front of the key gate: RTPS FLAG (CR31) sign bit must be clear, and the per-axis bit-11 wrap test must pass on all three axes.
- **dqa:** NONE. This emitter never touches DQA/DQB and never runs a depth cue — FUN_800329E0 is not called and MAC0 is never read. Colour is fixed, not depth-modulated.
- **scaleFormula:** NONE — there is no sprite scale. The primitive's size is purely screen-space: the streak runs from the projected head (SXY2) to 2*prev - cur, i.e. its length is twice the mote's per-frame screen displacement, and its width is the GPU's fixed 1px line.
- **ir0Formula:** NONE (0x1F800090 is never written or read; no DPCS/depth cue in this member).
- **farColour:** NONE — CR21-23 are never written and never used. The two vertex colours are literals: head = 0xFFFFFF (white, the current position), tail = 0x808080 (mid grey, the extrapolated end), gouraud-interpolated along the streak and blended 0.5*B + 0.5*F by the DR_MODE's tpage-embedded semi-transparency mode 0.
- **recordListFormula:** NONE — there is no record list. What plays the analogous role is the LCG lattice: seed = *(u32*)(node+0x50) read once, multiplier K = *(u32*)0x801450D8, stepped seed = seed*K + 1 three times per mote, with axis X reading (s32)seed>>16 BEFORE the first step, Y after the first, Z after the second. Mote coordinate = ((axisRand + base_axis) & 2047) where base_axis = (s16)node[anchor_axis] - offs_axis.
- **perFrameCount:** Exactly 32 motes are evaluated per node per frame; each that passes all four gates emits ONE line stroke (32 bytes of packet pool: 20 + 12). Upper bound 32 strokes / 1024 pool bytes per node per frame. A mote whose history slot holds the 0x7FFF sentinel consumes no pool at all.

### Helpers still needed

- `0x80084660` (12 guest instructions (5 mem_r32 + 5 gte_write_ctrl), no calls, no branches): libgte SetRotMatrix — loads MATRIX m[3][3] into GTE CR0-4. Small pure helper, portable now, but the native producer does NOT need it (projComposeCamera supplies the same rotation). Only worth owning if some other port wants the guest-side GTE path.
- `0x80084690` (9 guest instructions (3 mem_r32 + 3 gte_write_ctrl), no calls, no branches): libgte SetTransMatrix — loads MATRIX t[0..2] (bytes +20/24/28) into GTE CR5-7. Small pure helper, portable now, not needed by the native producer.
- `0x80083DE0` (39 guest instructions; the existing draft is ~35 lines of C): libgpu SetDrawMode. An existing static draft in game/render/wide_re_libgpu_leaves.cpp:280 has a1 and a3 SWAPPED versus the guest-visible behavior (gen: tpage bits from a3, 0x400 gate from a1). It is unwired so the bug is latent, but it is indexed LIVE in docs/codemap.md (address index) and the file's block comment asserts a3 is unused. Fixing that comment+body is a small correction that belongs to whoever next touches that file — the rain producer itself needs no equivalent, only the constant blend mode 0.
- `n/a (host state)` (~30 lines including the real-frame/interp-frame guard): Per-node streak history on the Render side: 32 x { float px, py; bool valid; } plus the previous cube base, refreshed on real frames only. This is the read-only replacement for the guest's 0x801485E8 array and node+0x48/0x4A/0x4C. Needs a keying decision (single instance vs keyed by node pointer) — see risks.

### Proposed wrappers

- struct RainMoteNode — a typed lens over the type-0x20 node with named accessors: anchorX()/anchorY()/anchorZ() (s16 at +0x2C/+0x2E/+0x30), prevBaseX()/prevBaseY()/prevBaseZ() (s16 at +0x48/+0x4A/+0x4C), lcgSeed() (u32 at +0x50). No mem_r16(node + 0x2C) at any call site.
- struct WrapCube { int32_t cx, cy, cz; } + WrapCube rainWrapCube(const EObjXform& cam, Core* c) — computes the camera-locked cube centre: T2_CAM_POS_* (unsigned high halfword) - kCubeHalfSpan + halfForward(cam.R[2][i]). Named constants kCubeSpan = 2048, kCubeMask = 0x7FF, kCubeHalfSpan = 1024.
- static inline int32_t halfForwardRow(float m) — the (v << 11) >> 12 idiom named for what it IS: floor(m / 2) of a 1.3.12 rotation-matrix element. The << 11 >> 12 form must not survive into the port as a shift pair.
- struct MoteLcg { uint32_t s; uint32_t mult; int32_t hi() const { return (int32_t)s >> 16; } void step() { s = s * mult + 1u; } } — so the 'X reads before the first step, Y after the first, Z after the second' ordering is expressed as hi()/step() calls in the guest's order rather than as three inlined multiplies.
- SpriteAnchor::otKeyRaw(int sz) -> int (returns the bucket key or -1) as a sibling of the existing otKeyInRange, implementing the NO-PRE-CLAMP variant: e = sz >> 12; k = ((sz >> 2) >> (e & 31)) + (e << 9); return ((uint32_t)(k - 4) < 0x7FCu) ? k : -1. Named constants kOtKeyMin = 4, kOtKeyMax = 0x7FF. This makes the difference from the sprite family's clamped gate explicit instead of silently reusing the wrong one.
- static inline bool moteWrapped(int32_t cur, int32_t prev) { return ((cur ^ prev) & kWrapTestBit) != 0; } with kWrapTestBit = 0x800 — names the bit-11 rollover test so the reason a streak vanishes is readable.
- constexpr float kStreakOvershoot = 2.0f and a named tailPoint(head, prev) helper returning head + kStreakOvershoot * (prev - head), so the '2*prev - cur' extrapolation is stated as an authored streak-length choice, not an opaque shift-and-add.
- constexpr unsigned char kStreakHeadGrey = 255, kStreakTailGrey = 128 and constexpr int kStreakBlend = 0 (0.5*B + 0.5*F, decoded from the DR_MODE word 0xE1000215) — the decoded meaning of 0x52FFFFFF / 0x00808080 / the tpage bits, in the same spirit as fx_line.cpp's kRopeBlend.
- constexpr uint32_t kRainLcgMult = 0x801450D8u, kRainStreakHistory = 0x801485E8u, kRainMoteCount = 32 — named A08 overlay-data addresses, in the file's anonymous namespace, matching fx_sprite.cpp's kFieldLcgMult / kFieldRecTable convention.
- A short enum or named constants for the sibling behaviour's suppression gates if the producer chooses to honour them defensively: kAreaCollectedBits = 0x800BFE56 / kArea8Bit = 0x100, kPauseFreezeFlag = 0x800BF816 — though render_walk's node+1 visibility check already covers both.

### Risks

- PREVIOUS-FRAME STATE OWNERSHIP is the real design risk. The effect is differential and the guest keeps its history in guest memory (0x801485E8 and node+0x48/0x4A/0x4C). pc_render must not write either, and READING them is order-dependent: the substrate's own render walk also calls 0x80116904 each frame, so by the time Render::fieldObjectsRender runs the guest slots may already hold THIS frame's values, not the previous frame's. I did not verify that ordering statically. The safe construction is a host-side shadow updated on real frames only — but that changes what 'previous' means on the very first frame after the effect resumes, where the guest resets its history to the sentinel and the host shadow would not know.
- THE HISTORY BUFFER IS SHARED, NOT PER-NODE. 0x801485E8 is one fixed 32-entry array in overlay A08 data, addressed from a constant, with no node keying. If two nodes carrying render fn 0x80116904 are ever alive at once they overwrite each other's streak history every frame. Static reading cannot tell me whether area 8 ever spawns more than one. A host-side shadow keyed by node pointer would be MORE correct than the guest — which is a divergence from the guest's own (buggy or single-instance-by-construction) behaviour. Decide deliberately and note it.
- THE RAIN IDENTIFICATION IS INFERENCE, not a sighting. It rests on: sibling behaviour 0x8011B398 advancing the anchor by (-12, +101, +56) per frame, and +Y meaning DOWN in this engine (evidence: game/render/fx_line.cpp's kTetherRise comment 'the +100 lands below it' and kTetherDrop 'straight down 400 units'). White-to-grey semi-transparent streaks of doubled length falling at 101 units/frame is rain, but nobody has looked at area 8 with this producer on. Verify by eye before writing 'rain' into a doc as fact.
- OT KEY vs NATIVE DEPTH. The guest's logarithmic key is used for TWO things: the emit/skip gate and the draw order. The gate must be reproduced exactly (with the no-pre-clamp variant); the ORDER must NOT be — per CLAUDE.md the engine owns ordering, so use proj_pz_to_ord(pv.pz) like every other producer in fx_line.cpp/fx_sprite.cpp. Reusing SpriteAnchor::otKeyInRange verbatim silently changes the gate for SZ3 in [0,15].
- gte_write_data(1, *(u32*)0x1F8000C4) loads VZ0 from a 32-bit read whose UPPER halfword (0x1F8000C6) is never written by this function and holds stale scratch. On real PSX hardware VZ0 is 16-bit and the upper half is ignored, so the native producer using vz alone is right — but if anything ever compares GTE data-reg state against the substrate leg, that stale half is a legitimate difference and not a port bug.
- The tail extrapolation is computed on the 16-bit SXY2 halfwords with u16 wraparound (tailX = (u16)curX + 2*((s16)prevX - (s16)curX), stored back to a u16 slot), and SXY2 itself is the GTE's clamped +-1024 screen coordinate. A native producer working in floats from pv.px/pv.py will NOT reproduce the guest's wrap on a long streak whose extrapolated end runs far off screen. That is almost certainly an improvement (the guest would draw a wrapped garbage line), but it is a deliberate divergence, not a match.
- cam.R[2][*] is read through the fps60-lerped Fps60::sceneCam while the camera POSITION (0x1F8000D2/D6/DA) is read raw from the scratchpad. The two feed the wrap-cube offset `offs`, which cancels out of the mote's world position except through the 11-bit mask — so this should not jitter, but it does mean a mote can wrap one interp frame earlier or later than the guest would.
- I could not find the code that installs 0x80116904 into node+0x18 — the pointer is not an immediate anywhere in the A08 shards, so it comes from an overlay data table. That means I could not confirm from the binary how many such nodes exist, nor cross-check the node layout against a second consumer. The layout above is derived entirely from this function plus its sibling behaviour 0x8011B398, both of which agree.
- guest 0x80083DE0 in game/render/wide_re_libgpu_leaves.cpp:280 disagrees with its own guest-visible behavior (a1/a3 swapped — see calls[]). It is static and unwired so nothing is broken today, but docs/codemap.md (address index) lists 0x80083DE0 as LIVE at that line and the block comment asserts a3 is unused, which is false. Worth a separate finding; do not build the rain port on that comment.


---

## 0x80110CA4 — area 14 (A0E overlay)

**Portable:** moderate — The grid half is straightforward: fixed model-space geometry with no per-item state, no RNG, no LCG, no animation script — 80 quads whose only frame-varying inputs are the node's rot/pos, the tick-derived texture offsets, and one global predicate. Every prologue callee is already native (Math::rotmat / matMul / applyMatlv), the emit is a plain gouraud-textured quad that RenderQueue::drawWorldQuad already supports with per-vertex colour, and there is an exact structural precedent in game/render/swing_fx.cpp (AVSZ4 -> otBucket gate -> world quad). Three things push it above 'straightforward': (1) Math::rotmat's only signature WRITES the matrix to a guest address, so a read-only producer needs a host-output variant extracted from it; (2) the guest culls on the GTE FLAG register (CR31 bit31), which a float projection has no equivalent for and which WILL fire often on a plane this large; (3) the OT key carries a deliberate +30/+40 row-dependent bias and a 2045 clamp that authors sort order the depth buffer will not reproduce. And the render fn is only half done until 0x801104D0 (440 gen lines, sprite family) is also ported.

**Effect:** A large animated textured BACKDROP PLANE with a mirrored reflection and a glow band along its seam. Evidence: (a) grid A is a single flat plane 12000 x 16800 model units at local Z = 0, rotated 90 degrees about Y (node+0x4A init 1024) so it stands as a wall in the world Y-Z plane facing along X, positioned at (2290, -2761, 9216) — far too big and too regular to be an object, and exactly the shape of a sky/sea backdrop; (b) the texture V coordinate is MIRRORED about the row y = 0 (v0,v1 = vBase / v2,v3 = vBase+62 above, exactly reversed below), which is the signature of a reflection, not a scroll; (c) the vertex colours ramp from neutral grey 0x808080 above the seam, through a bright near-white 0xDFDFDF on the seam edge itself, into a dark blue-black 0x00201000 below it, with one transition row at y = 2400 — a horizon line with a lit waterline and darkening depth; (d) grid B lays 10 additive quads (code 0x3E plus tpage semi mode 1 = B+F) flat at y = 0 extending 1200 units perpendicular to the wall, grey at the seam fading to black — a haze/spray band hugging the waterline; (e) the texture animates as a 4-frame U cycle every 8 ticks over a 4-band V cycle every 32 ticks, i.e. a looping animated surface rather than a static image. Best reading: a body of water or a sky-over-water backdrop, with the reflection above/below the waterline and a mist strip along it. I did not run the game and the texture page contents are unknown, so the specific subject is a claim, not a measurement — the geometry, mirror, colour ramp and additive band are all directly readable from the instruction stream and are not.

**Summary:** 0x80110CA4 (overlay A0E, area 14) is NOT a member of the FUN_80027A4C sprite family — it is a direct POLY_GT4 (13-word / 52-byte gouraud-textured-quad) GRID EMITTER that writes packets into the shared pool and links them into the OT itself. Its prologue composes a per-node view matrix (Math::rotmat of the node's Euler angles at node+0x48, Math::matMul against the scene camera at scratchpad 0x1F8000F8, Math::applyMatlv of the node's world position at node+0x2C, then add the camera translation), CTC2s it to GTE CR0-7, zeroes the far colour CR21-23 and zeroes the scratchpad depth-cue slot 0x1F800090. It then draws two grids in a fixed model space. Grid A: 7 rows x 10 columns = 70 quads, each 1200 wide (local X, spanning [-6000,6000]) x 2400 tall (local Y, spanning [-9600,7200]) at local Z = 0 — a big flat backdrop plane, gouraud-shaded neutral grey 0x808080 for rows at y<=0, a bright 0xDFDFDF seam on the y=0 edge, and a dark blue-black 0x00201000 for rows below, with the texture V-coordinate MIRRORED about y=0. Grid B: 10 more quads, all at local Y = 0, spanning local Z in [0,1200] — a horizontal additive (code 0x3E, tpage semi-mode 1) haze band running along the seam, grey at the seam fading to black 1200 units out. Every quad is RTPT'd (V0,V1,V2) + RTPS'd (V3), gated on the GTE FLAG register, on a screen bounding test (some vertex X in [0,320] AND some vertex Y in [0,240]), and on an OT-bucket range gate over the AVSZ4 average depth. After both grids it stores the advanced pool pointer back to 0x800BF544 and TAIL-CALLS 0x801104D0 with the same node — a second, unported 440-line emitter that IS a sprite-family member (FUN_800329E0 / FUN_800317CC / FUN_80027A4C + the RNG at 0x8009A450). So this render fn has two halves and porting only the grid leaves the sprite half dark.


### Verifier corrections (CORRECTED)

- **[MAJOR]** claim: newHelpersNeeded #2: "Math::rotmat currently only exists as rotmat(uint32 anglesPtr, uint32 out) which STORES the matrix into guest memory... Extract the pure computation into a host-output helper — e.g. Math::rotmatHost(int16 ax, int16 ay, int16 az, int16 m[3][3])" — presented as ~30 lines of new work the port needs.

  actual: The host-output variant ALREADY EXISTS and is LIVE: `MeshQuads::rotmat(Core* c, int16_t ax, int16_t ay, int16_t az, int32_t M[3][3])` at game/render/mesh_quads.cpp:62, declared game/render/mesh_quads.h:21 with the comment "Math::rotmat (FUN_80085480) element math on three Euler angles, into a 1.3.12 3x3". It uses the identical GPF-clamped element math (mesh_quads.cpp:35 `gpf()` == gte_math.cpp `gpf1()`), takes the three angles as host int16 and writes a HOST array — zero guest writes. The spec's own `calls` entry even cites this exact file as the codemap DUAL-OWNERSHIP partner for 0x80085480 without noticing it is the helper it is asking for. Following the spec produces a THIRD implementation of 0x80085480 in a codebase whose codemap already errors on two. The established call shape is game/render/margin_render.cpp:160-165 (host Robj scaled to ~4096 -> projComposeObjectHost); MeshQuads::rotmat already emits 1.3.12 (~4096) scale, so it feeds projComposeObjectHost directly with only a float cast.

- **[MINOR]** claim: Summary + EPILOGUE step + calls[0x801104D0]: "TAIL-CALLS 0x801104D0 with the same node" / "TAIL CALL with a0 = the same node, after the pool cursor is stored back".

  actual: It is an ordinary JAL, not a tail call. Line 4338: `c->mem_w32((c->r[2] + (uint32_t)-2748), c->r[18]); overlay guest 0x801104D0(c);` with `c->r[31] = 0x801113F4u` set on the preceding line, followed by the full callee-save restore block (r31/r22/r21/r20/r19/r18/r17/r16 from sp+84..sp+56) and `c->r[29] += 88; return;`. Consequence for a port: 0x80110CA4 owns a live 88-byte stack frame across the call, so a native producer replacing the WHOLE fn must mirror the frame (CLAUDE.md "MIRROR THE GUEST STACK"); a genuine tail call would have let the callee inherit the frame.

- **[MINOR]** claim: nodeFields +0x2E: "posY — world Y of the plane's origin (init -2761); the tick fn animates it".

  actual: The tick fn does NOT animate node+0x2E. `overlay guest 0x80110268` (authenticated executable/overlay evidence) writes node+46 exactly once, in the one-shot init branch (`r2 = -2761; mem_w16(node+46, r2)`); at L_80110480 it only READS node+46, subtracts 500, and stores the result into a stack SVECTOR for the call to 0x8006CBA8 — node+46 is never written again. The field the tick fn animates is node+0x2C (posX): state-1 adds +10/tick clamped to 6460 (L_80110358), state-3 adds +8/tick clamped to 9090 (L_8011041C). So the plane translates along its X, not its Y. The spec's +0x2C entry does note the dual reading but attributes the animation to the wrong field, which inverts what a native producer must lerp for fps60.

- **[MINOR]** claim: risks[0]: the GTE FLAG cull "ORs in MAC1-3 overflow, SX2/SY2 saturation (screen coords outside +/-1024), SZ3 saturation and divide overflow (H/SZ3 > 0x1FFFF)".

  actual: CR31 bit31 is the OR of bits [30:23] and [18:13]. That list omits two contributors the spec's enumeration would have a porter ignore: IR1 saturated (bit24) and IR2 saturated (bit23) — which fire whenever a projected MAC1/MAC2 leaves +/-32767 with lm=0, entirely plausible on a 12000x16800-unit plane — and MAC0 overflow (bits 16/15). Conversely bit22 (IR3 sat) and bits 21..19 (colour FIFO sat) are NOT in the error flag. Any attempt to reproduce the cull natively must gate on the right bit set.

- **[MINOR]** claim: portable.reason + emitContract.writer: "there is an exact structural precedent in game/render/swing_fx.cpp (AVSZ4 -> otBucket gate -> world quad)" and "game/render/swing_fx.cpp is the closest working precedent" for a RenderQueue::drawWorldQuad submit.

  actual: swing_fx.cpp is the WRONG precedent on two counts, and its own header says so. game/render/swing_fx.h:30-35: "WHY emitOrQueue AND NOT drawWorldQuad (this cost a whole debug cycle, do not undo it): a prim submitted through drawWorldQuad carries has_xyf = 1, and Fps60::isTier1Owned treats EVERY RQ_WORLD prim with has_xyf = 1 as owned by the display-pass re-render". Second, swing_fx.h:23-27 states the producer is "A SCOPED LEAF TAP on the shared mesh emitter... it runs the untouched guest-visible behavior" — i.e. exactly the transitional tap the project directives ("A tap is a SCAFFOLD, not the destination") say must be retired, not copied. drawWorldQuad IS the right call for THIS producer because it runs at walk time, not guest-execution time — but the precedents are game/render/fx_sprite.cpp:188 / fx_ring.cpp / mesh_quads.cpp:136, not swing_fx. Only swing_fx's file-local `otBucket()` (swing_fx.cpp:38) transfers, and it is byte-identical to grid B's gate only.


### Algorithm (as CORRECTED by the verifier — a DELTA; read with the ORIGINAL below)

1. Steps 1 through 16 of the spec's `algorithm` array are CORRECT AS WRITTEN — I re-derived every address, shift, mask, branch polarity, loop bound, delay-slot induction and packet offset independently and found no discrepancy. Use them verbatim. Only the final EPILOGUE step and the surrounding non-algorithm claims need the corrections below.
2. EPILOGUE (replaces the spec's last step). mem_w32(0x800BF544, pkt) stores the advanced pool cursor back. Then an ORDINARY CALL (JAL, ra = 0x801113F4) to 0x801104D0 with a0 = the same node — NOT a tail call. On return, restore r31/r22/r21/r20/r19/r18/r17/r16 from sp+84/80/76/72/68/64/60/56, sp += 88, return. PORT CONSEQUENCE: 0x80110CA4 holds a live 88-byte guest frame across that call, so if the port replaces the whole render fn (rather than being a read-only pc_render overlay running alongside the substrate body) it must mirror the frame per CLAUDE.md 'MIRROR THE GUEST STACK'. As a read-only pc_render producer wired into render_walk.cpp's type-0x20 chain, the substrate body still runs underneath and the frame is not the port's concern.
3. HELPER CORRECTION (replaces newHelpersNeeded #2). Do NOT write a new Math::rotmatHost. Call the EXISTING MeshQuads::rotmat(c, (int16_t)rotX, (int16_t)rotY, (int16_t)rotZ, int32_t M[3][3]) — game/render/mesh_quads.h:21, implementation mesh_quads.cpp:62 — which is already the host-output form of FUN_80085480 with identical GPF-clamped element math and zero guest writes. Its output is 1.3.12 (~4096) scale, exactly the Robj convention projComposeObjectHost expects (see game/render/margin_render.cpp:160-165), so the compose is: MeshQuads::rotmat -> cast to float Robj[3][3] -> Render::projComposeObjectHost(Robj, Tobj = {node+0x2C, +0x2E, +0x30}, &xf) -> xf.project(...) per vertex. The pre-existing DUAL-OWNERSHIP between Math::rotmat and MeshQuads::rotmat should be consolidated separately; adding a third body would make it worse.
4. NODE-FIELD CORRECTION. node+0x2E (posY) is written ONCE, in the tick fn's one-shot init (= -2761), and never again. The animated field is node+0x2C: state 1 adds +10 per tick clamped to 6460, state 3 adds +8 per tick clamped to 9090 — and that same halfword is the sequence-progress scalar 0x801104D0 tests against 9039 / 7801.. / 7501... So the plane translates along its local/world X, and that is the field an fps60 actor-transform lerp must interpolate.
5. PRECEDENT CORRECTION. Model the producer on game/render/fx_sprite.cpp (fxRingSpriteRender / fxParticleFieldRender), fx_ring.cpp and mesh_quads.cpp:136 — walk-time native producers that submit via RenderQueue::drawWorldQuad. Do NOT model it on swing_fx.cpp: swing_fx.h:23-35 documents that it is a scoped LEAF TAP that runs the guest-visible behavior and deliberately uses emitOrQueue, NOT drawWorldQuad ('this cost a whole debug cycle, do not undo it'). The only thing to reuse from swing_fx is its file-local otBucket() log map, which matches GRID B's gate exactly and grid A's only up to the log step.
6. FLAG-CULL CORRECTION (risks[0]). CR31 bit31 = OR of bits [30:23] and [18:13]. Contributors are: MAC1-3 positive/negative overflow (30..25), IR1 saturated (24), IR2 saturated (23), SZ3/OTZ saturated (18), divide overflow H/SZ3 > 0x1FFFF (17), MAC0 overflow (16/15), SX2 saturated to [-1024,1023] (14), SY2 saturated (13). NOT contributors: IR3 saturation (22) and colour-FIFO saturation (21..19). Any native approximation of the cull must include the IR1/IR2 saturation and MAC0 terms the spec's list omits.

### Algorithm (ORIGINAL spec steps — the corrections above are stated against these)

1. PROLOGUE, step 1 — tick-derived texture offsets. tick = (u16)mem_r16(0x1F80017C); t2 = tick >> 1. uBase = (t2 & 3) << 6  -> {0,64,128,192}. vBase = ((t2 << 4) & 192)  ==  ((t2 >> 2) & 3) << 6 -> {0,64,128,192}. So U advances one 64-texel step every 2 ticks (cycle 8), V one 64-texel step every 8 ticks (cycle 32). Both are computed ONCE, before the matrix work, and never mutate inside the loops.
2. PROLOGUE, step 2 — view matrix. Math::rotmat(node+0x48, 0x1F800000): builds the node's 3x3 from Euler angles (vx = s16@node+0x48, vy = s16@node+0x4A, vz = s16@node+0x4C) into the scratchpad workspace at 0x1F800000 (5 words, CR packing). Then Math::matMul(0x1F8000F8 /*camera rotation*/, 0x1F800000 /*node rotation*/, 0x1F800000 /*out*/) -> work = Rcam x Rnode (>>12, int16-clamped per element). Then Math::applyMatlv(node+0x2C, 0x1F800014) -> the node's world position (VX = s16@node+0x2C, VY = s16@node+0x2E, VZ = s16@node+0x30) rotated by the matrix now in CR0-4 (= Rcam, left there by matMul), stored int16-clamped and sign-extended into the workspace translation slots 0x1F800014/18/1C. Then, elementwise, work.TR += camera.TR read from 0x1F80010C / 0x1F800110 / 0x1F800114 (= 0x1F8000F8 + 0x14/0x18/0x1C).
3. PROLOGUE, step 3 — GTE setup. CTC2 CR0..CR4 from 0x1F800000..0x1F800010 (composed rotation), CR5/CR6/CR7 from 0x1F800014/18/1C (composed view translation). CR21 = CR22 = CR23 = 0 (far colour cleared). mem_w32(0x1F800090, 0) — the depth-cue IR0 publish slot zeroed (identity cue; consumed by the 0x801104D0 tail, not by the grid). Note ZSF4 (CR30) and OFX/OFY/H (CR24-26) are NOT touched here — they are whatever the scene frame published.
4. PROLOGUE, step 4 — pool cursor. pkt = mem_r32(0x800BF544) — the shared packet-pool bump pointer. Loop-local alias r5 = pkt + 7 (points at the POLY_GT4 code byte). Both advance by 52 per EMITTED quad only; culled quads do not advance.
5. GRID A LOOP BOUNDS. Outer row y: y = -7200; body; y += 2400; repeat while (int)y < 7201  -> bodies at y = -7200,-4800,-2400,0,2400,4800,7200 (7 rows). Inner column x: reset x = -6000 at every row start; body; x += 1200; repeat while (int)x < 6000 -> bodies at x = -6000..4800 step 1200 (10 columns). Column index col is reset to 0 at each row start and incremented in the branch delay slot at the END of each column body, so during the body col = 0-based column index. 70 quads max.
6. GRID A VERTICES (model space, all Z = 0). V0 = (x, y-2400, 0); V1 = (x+1200, y-2400, 0); V2 = (x, y, 0); V3 = (x+1200, y, 0). All coordinates stored as s16 halfwords. Z-order quad: V0 top-left, V1 top-right, V2 bottom-left, V3 bottom-right in local (X, Y).
7. GRID A PROJECTION + GATE 1. Load VXY0/VZ0 = V0, VXY1/VZ1 = V1, VXY2/VZ2 = V2 and run GTE RTPT (opcode 0x4A280030, cmd 0x30, sf=1, lm=0). Read CR31 (FLAG); if (int32)FLAG < 0 -> SKIP this quad entirely. Otherwise store SXY0 (data 12) -> pkt+8, SXY1 (data 13) -> pkt+20, SXY2 (data 14) -> pkt+32. Then load VXY0/VZ0 = V3, write tpage = 45 to pkt+26, run RTPS (0x4A180001, cmd 0x01, sf=1). Read CR31 again; if < 0 -> SKIP. Otherwise store SXY2 (data 14) -> pkt+44. NOTE the FIFO ordering: after RTPT + RTPS the SZ FIFO holds SZ0 = z(V0), SZ1 = z(V1), SZ2 = z(V2), SZ3 = z(V3).
8. GRID A GATE 2 — screen bounding test, read back out of the packet as UNSIGNED 16-bit. X test: pass if ANY of u16(pkt+8), u16(pkt+20), u16(pkt+32), u16(pkt+44) is < 321 (a negative s16 reads as >= 32768 and therefore fails). If none pass -> SKIP. Y test: pass if ANY of u16(pkt+10), u16(pkt+22), u16(pkt+34), u16(pkt+46) is < 241. If none pass -> SKIP. The two tests are SEPARATE 'any' tests, not a per-vertex conjunction. On passing, write clut = 16190 (0x3F3E) to pkt+14.
9. GRID A DEPTH KEY — exact order, do not reorder. (a) GTE AVSZ4 (0x4B68002E, cmd 0x2E): otz = (ZSF4 * (SZ0+SZ1+SZ2+SZ3)) >> 12, Lm_D-saturated to [0,65535], read from data reg 7. (b) phase bias: if ((uint32)mem_r32(0x800BF850) < 2) otz -= 80; else otz += 120. (c) small-value snap: if ((uint32)(otz + 39) < 43)  i.e. otz in [-39, 3]  -> otz = 5. (d) LOG COMPRESSION: b = (int32)otz >> 10; key = ((int32)otz >> (b & 31)) + (b << 9). (e) if ((int32)key < 4) key = -1. (f) if ((int32)key >= 2048) key = 2047. (g) if ((int32)key < 0) -> SKIP. (h) ROW BIAS APPLIED AFTER THE LOG MAP: key += (y > 0 ? 40 : 30)  — y is the OUTER row variable, so the whole lower half of the plane is pushed 10 buckets further back than the upper half. (i) if ((int32)key >= 2045) key = 2045.
10. GRID A TEXTURE U — column parity, from col (the inner index). If (col & 1) != 0: u0 = u2 = uBase, u1 = u3 = uBase + 32. If (col & 1) == 0: u0 = u2 = uBase + 32, u1 = u3 = uBase + 63. All written as bytes at pkt+12 (u0), pkt+24 (u1), pkt+36 (u2), pkt+48 (u3). Net effect: a continuous 64-texel tile spanning every 2 columns.
11. GRID A TEXTURE V + COLOURS — row-half dependent, and V is MIRRORED about y = 0. If y <= 0 (upper half): v0 = v1 = vBase, v2 = v3 = vBase + 62; rgb0 = rgb1 = 0x00808080; rgb2 = rgb3 = (y == 0 ? 0x00DFDFDF : 0x00808080). If y > 0 (lower half): v3 = v2 = vBase, v1 = v0 = vBase + 62; if (y == 2400) { rgb0 = rgb1 = 0x00808080; } else { rgb0 = rgb1 = 0x00201000; } and rgb2 = rgb3 = 0x00201000 in both cases. V bytes at pkt+13 (v0), pkt+25 (v1), pkt+37 (v2), pkt+49 (v3). RGB words at pkt+4, pkt+16, pkt+28, pkt+40 (24-bit BGR-in-word: byte0 = R, byte1 = G, byte2 = B; 0x00201000 = R 0x00, G 0x10, B 0x20 — a dark blue).
12. GRID A EMIT. Write the code byte pkt+7 = 60 (0x3C = POLY_GT4, opaque) — this MUST come after the rgb0 word write, which zeroes that byte. OT link: otbase = mem_r32(0x800ED8C8); slot = otbase + key*4; mem_w32(pkt+0, mem_r32(slot) | 0x0C000000); mem_w32(slot, pkt). Then pkt += 52.
13. GRID B LOOP. col2 = 0; x = -6000; body; x += 1200; repeat while (int)x < 6000 -> 10 quads. Vertices, all at local Y = 0: V0 = (x, 0, 0); V1 = (x+1200, 0, 0); V2 = (x, 0, 1200); V3 = (x+1200, 0, 1200). Same RTPT+RTPS+FLAG sequence, same tpage 45 write, same screen bounding test, same clut 16190.
14. GRID B DEPTH KEY — SHORTER than grid A's, do not copy A's. (a) otz = AVSZ4. (b) same phase bias: if ((uint32)mem_r32(0x800BF850) < 2) otz -= 80; else otz += 120. (c) SAME log compression b = otz>>10; key = (otz >> (b&31)) + (b<<9). (d) range gate: if ((uint32)(key - 4) < 2044) keep, else key = -1. (e) if ((int32)key < 0) -> SKIP. NO small-value snap to 5, NO row bias, NO 2045 clamp.
15. GRID B TEXTURE + COLOURS. U by column parity, identical rule to grid A on (col2 & 1). V is fixed: v0 = v1 = vBase, v2 = v3 = vBase + 62. Colours: rgb0 = rgb1 = 0x00808080, rgb2 = rgb3 = 0x00000000. Code byte pkt+7 = 62 (0x3E = POLY_GT4 with ABE set); combined with tpage 45's semi bits (mode 1) that makes it ADDITIVE (B + F). Same OT link + pkt += 52.
16. EPILOGUE. mem_w32(0x800BF544, pkt) — store the advanced pool cursor back. Then tail-call 0x801104D0 with a0 = the same node: the unported sprite-family half of this render fn.

### Node fields

| offset | type | name | meaning |
|---|---|---|---|
| `+0x01` | u8 | visible | per-frame visibility marker the walk tests before dispatching (render_walk.cpp: mem_r8(n+1) == 0 -> skip) |
| `+0x04` | u8 | initPhase | 0/1/2/3 init+run phase driven by the tick fn 0x80110268; the one-shot init branch sets it to 1 after seeding the fields below |
| `+0x05` | u8 | seqState | 0..4 sequence state, dispatched by 0x80110268 through the 5-entry jump table at 0x80109204. Not read by the render fn. |
| `+0x0B` | u8 | nodeType | 0x20 = custom-render-fn node (the walk's type gate) |
| `+0x18` | u32 | renderFn | guest render fn pointer = 0x80110CA4; first instruction 0x27BDFFA8 (addiu sp,-88) is the overlay-residency guard the walk should check |
| `+0x2C` | s16 | posX | world X of the plane's origin (init 2290). ALSO used by the 0x801104D0 tail as a 16-bit sequence-progress scalar (compared against 9039, 7801..8299, 7501..9199) — one field, two readings. |
| `+0x2E` | s16 | posY | world Y of the plane's origin (init -2761); the tick fn animates it |
| `+0x30` | s16 | posZ | world Z of the plane's origin (init 9216) |
| `+0x48` | s16 | rotX | Euler X, 12-bit angle (4096 = full turn). Init 0. |
| `+0x4A` | s16 | rotY | Euler Y. Init 1024 = 90 degrees, which maps the plane's local (X,Y) onto world (Z,Y) — a wall standing in the Y-Z plane facing along X. |
| `+0x4C` | s16 | rotZ | Euler Z. Init 0. |

### Globals

| addr | type | name | meaning |
|---|---|---|---|
| `0x1F800000` | MATRIX (5 rot words + 3 trans words) | kScrMatrixWork | SCRATCHPAD. (uint32)8064 << 16 = 0x1F800000. Composed-matrix workspace: rotation at +0x00..+0x13 (CR0-4 packing), TRX/TRY/TRZ at +0x14/+0x18/+0x1C. rotmat writes here, matMul overwrites in place, applyMatlv writes the rotated node position into the translation slots, then camera TR is added. Finally CTC2'd into CR0-7. A read-only native producer does NOT need this address at all — it composes in floats. |
| `0x1F80017C` | u16 (read as halfword) | kFrameCounter | SCRATCHPAD. 0x1F800000 + 380. The engine's frame/tick counter. Drives BOTH texture offsets: uBase = ((tick>>1) & 3) << 6, vBase = (((tick>>1) << 4) & 192). Already named kFrameCounter in game/render/field_hud.cpp and kFrameCtr in fx_vortex.cpp. |
| `0x1F8000F8` | MATRIX | kScrSceneCamera | SCRATCHPAD. 0x1F800000 + 248. The SCENE CAMERA view matrix: rotation at +0x00, translation TRX/TRY/TRZ at +0x14/+0x18/+0x1C = absolute 0x1F80010C / 0x1F800110 / 0x1F800114. The guest-visible behavior reaches the translation via a second base r16 = 0x1F8000F8 - 40 = 0x1F8000D0 plus offsets 60/64/68 — the SAME three words. Read natively through Fps60::sceneCam (projComposeCamera / projComposeCore). |
| `0x1F800090` | s32 | kScrDepthCueIr0 | SCRATCHPAD. 0x1F800000 + 144. The depth-cue IR0 publish slot the FUN_80027A4C record writer consumes. This fn WRITES 0 (identity cue). The grid never reads it; it is set for the 0x801104D0 tail. Already named kScrDepthCue in game/render/swing_fx.cpp. |
| `0x800BF544` | u32 | kPktPoolPtr | MAIN RAM (stable MAIN.EXE global). (uint32)32780 << 16 = 0x800C0000, offset -2748. The shared packet-pool bump-allocator write pointer. Read at entry, advanced 52 bytes per emitted quad, stored back before the tail call. Already named PKT_POOL_PTR / kPktPoolPtr in perobj_billboard.cpp, text_label.cpp, tile_grid_layer.cpp. |
| `0x800ED8C8` | u32 (pointer) | kOtBasePtr | MAIN RAM (stable MAIN.EXE global). (uint32)32783 << 16 = 0x800F0000, offset -10040. *this = the base of the ACTIVE ordering table; the OT slot is base + key*4. This emitter uses the active base directly, i.e. sub-bucket shift 0, so its guest sort key IS the log-compressed key. Already named OT_BASE_GLOBAL / OTBASE_PTR / kOtBaseGlobal elsewhere. |
| `0x800BF850` | u32 | kSeqStateWord | MAIN RAM (stable MAIN.EXE global). (uint32)32780 << 16 = 0x800C0000, offset -2040 (r15/r13 base 0x800BF808) plus 72. A small enumerated scene/sequence state word, written across 21 sites in the generated set: this area's own tick fn 0x80110268 stores 2 into it and elsewhere tests it against 1; MAIN.EXE's ActorSm40558 (0x80040390) decrements it; overlay guest 0x8010BCC8 compares it to 128. The ONLY thing this producer needs is the predicate (uint32)*word < 2, which selects the OT depth bias -80 vs +120. NOT the scene flag table (that is 0x800BF870) and not the area byte. |

### Callees

| addr | role | native equivalent |
|---|---|---|
| `0x80085480` | libgte RotMatrix — builds the node's 3x3 from the Euler angles at node+0x48 into the scratchpad workspace 0x1F800000. Called via typed runtime address dispatch with a0 = node+72, a1 = 0x1F800000. | Math::rotmat (game/math/gte_math.cpp:220) — LIVE. WARNING: codemap reports DUAL-OWNERSHIP with MeshQuads::rotmat (game/render/mesh_quads.cpp:62). ALSO: the existing signature is rotmat(uint32 anglesPtr, uint32 out) and it WRITES the result to a guest address, which a read-only pc_render producer must not do. The port needs a host-output extraction (see proposedWrappers). |
| `0x80084110` | 3x3 matrix multiply — work = Rcam x Rnode. Called with a0 = 0x1F8000F8 (camera rotation), a1 = 0x1F800000 (node rotation), a2 = 0x1F800000 (in-place out). Side effect the next call depends on: it CTC2-loads a0 (the camera rotation) into CR0-4. | Math::matMul (game/math/gte_math.cpp:123) — LIVE. For the port this is subsumed by Render::projComposeObjectHost / projComposeCore, which computes R = (Rcam . Robj)/4096 in float. |
| `0x80084220` | libgte ApplyMatrixLV-class MVMVA — rotates the node's world position (node+0x2C) by the matrix currently in CR0-4 (= the camera rotation left there by matMul) and stores int16-clamped, sign-extended into the workspace translation slots at 0x1F800014. | Math::applyMatlv (game/math/gte_math.cpp:351) — LIVE. Also subsumed by projComposeCore, which computes T = (Rcam . Tobj)/4096 + Tcam in float (see risks: the float path omits the int16 IR clamp). |
| `0x801104D0` | TAIL CALL with a0 = the same node, after the pool cursor is stored back. The SECOND HALF of this render fn: a genuine FUN_80027A4C sprite-family emitter (calls FUN_800329E0 to program the camera+DQA, FUN_800317CC(-50) as the anchor gate, FUN_80027A4C as the 8-byte record writer, and the RNG at 0x8009A450 heavily). It branches on a 5-way sequence state, reads u16 node+0x2C as a progress scalar against windows (9039, 7801..8299, 7501..9199), and drives a per-item table at 0x8012686C with per-item state at 0x80125BEC / 0x8012622C (stride 8) and a counter at 0x8011CE58. | NONE. codemap: 'NO native owner found'. 440 generated lines (authenticated executable/overlay evidence..3308). Its ONLY caller is 0x80110CA4, so it cannot be reached by whitelisting a different address — porting the grid alone leaves this layer dark. It is a big one, not a small pure helper; it should be a separate port task following the fxRingSpriteRender / fxParticleFieldRender pattern (it is the same family). |

### Emit contract

- **writer:** NEITHER family writer. This emitter builds POLY_GT4 packets itself: 13 words / 52 bytes, prim-length nibble 0x0C in the tag. Layout from the packet base P: P+0 tag, P+4 rgb0 (byte0 R, byte1 G, byte2 B, byte3 code), P+8 xy0, P+12 u0, P+13 v0, P+14 clut(u16), P+16 rgb1, P+20 xy1, P+24 u1, P+25 v1, P+26 tpage(u16), P+28 rgb2, P+32 xy2, P+36 u2, P+37 v2, P+40 rgb3, P+44 xy3, P+48 u3, P+49 v3. Code byte = 60 (0x3C, opaque) for grid A and 62 (0x3E, ABE/semi-transparent) for grid B. tpage = 45 (0x2D: page X base 13*64 = 832, Y base 0, semi mode 1 = B+F additive, 4-bit CLUT). clut = 16190 (0x3F3E: CLUT at x = 992, y = 252). Native equivalent: RenderQueue::drawWorldQuad(c, px, py, depth, u, v, r, g, b, tp=45, clut=16190, semi, nullptr) — it already takes per-vertex r/g/b, which is what this gouraud quad needs; game/render/swing_fx.cpp is the closest working precedent.
- **gateBias:** NOT a FUN_800317CC anchor bias — this emitter runs its own gate. Three stages: (1) GTE FLAG (CR31) bit31 after RTPT and again after RTPS; (2) a screen bounding test read back out of the packet as unsigned halfwords — pass if ANY of the four X in [0,320] AND (separately) ANY of the four Y in [0,240]; (3) the OT-bucket range gate. The only additive bias is the phase bias on otz: -80 when (uint32)*0x800BF850 < 2, else +120, applied BEFORE the log compression. Grid A then adds a POST-compression row bias of +40 when the row y > 0 and +30 otherwise.
- **dqa:** N/A — this emitter never programs DQA/DQB and never calls FUN_800329E0. There is no depth-cue-as-scale trick here; the geometry is real 3-D quads. (DQA is programmed by the 0x801104D0 tail, which is a separate port.)
- **scaleFormula:** N/A — no sprite scale. Geometry comes straight from the model-space grid: grid A cell 1200 (local X) x 2400 (local Y) at local Z = 0, x in [-6000,6000], y in [-9600,7200]; grid B cell 1200 (local X) x 1200 (local Z) at local Y = 0, x in [-6000,6000]. Projection is the ordinary RTPT/RTPS through the composed camera x node matrix, i.e. EObjXform::project.
- **ir0Formula:** IR0 = 0. The prologue explicitly writes 0 to 0x1F800090 (the depth-cue publish slot). The grid never runs DPCS/DPCT anyway — its vertex colours are written as raw 24-bit constants. So no depth cue: pass the colours through unchanged.
- **farColour:** CR21/CR22/CR23 = 0,0,0 — the prologue clears them. Irrelevant to the grid (no depth cue applied), but it is the far colour the 0x801104D0 tail's record writer will see.
- **recordListFormula:** N/A — no record list. The packet address is the shared pool cursor: P = mem_r32(0x800BF544) at entry, then P += 52 per EMITTED quad (culled quads do NOT advance it). The final P is stored back to 0x800BF544 before the tail call. OT link per quad: slot = mem_r32(0x800ED8C8) + key*4; mem_w32(P+0, mem_r32(slot) | 0x0C000000); mem_w32(slot, P). A read-only native producer does none of this — it hands the quad to drawWorldQuad and, if it wants the guest sort order, passes the same key as sort_key (sub-bucket shift is 0 because the active base is used directly).
- **perFrameCount:** Up to 80 POLY_GT4 per frame from this fn: 70 from grid A (7 rows x 10 columns) plus 10 from grid B, minus whatever the FLAG / screen-bounds / OT-range gates cull. Fixed count, no RNG, no per-item state — the only frame-to-frame variation is the node's rot/pos, the tick-derived U/V offsets and the *0x800BF850 predicate. The 0x801104D0 tail adds its own sprite records on top.

### Helpers still needed

- `0x801104D0` (440 generated lines (authenticated executable/overlay evidence)): The sprite-family SECOND HALF of this render fn — tail-called with the same node and reachable from nowhere else. Uses FUN_800329E0 (camera+DQA), FUN_800317CC(-50) (anchor gate), FUN_80027A4C (8-byte record writer), the RNG at 0x8009A450, a 5-way sequence-state branch on progress windows over u16 node+0x2C, an item table at 0x8012686C with per-item 8-byte state at 0x80125BEC and 0x8012622C, and a counter at 0x8011CE58. NOT a small pure helper — a full producer in its own right, same shape as Render::fxRingSpriteRender / Render::fxParticleFieldRender. Port it as a separate task; until then area 14 keeps a missing layer even after the grid lands.
- `0x80085480 (host-output variant, not a new guest fn)` (~30 lines extracted from game/math/gte_math.cpp:220-244, no new math): Math::rotmat currently only exists as rotmat(uint32 anglesPtr, uint32 out) which STORES the matrix into guest memory and also writes GTE data-reg leftovers. pc_render must not do either. Extract the pure computation (the six rotmat_trig lookups, the four GPF-rounded products, the nine truncated matrix elements) into a host-output helper — e.g. Math::rotmatHost(int16 ax, int16 ay, int16 az, int16 m[3][3]) — and have the existing Math::rotmat call it and then do its guest store + GTE leftovers. That keeps one implementation and gives the producer a clean float Robj for projComposeObjectHost. Fold the MeshQuads::rotmat dual-ownership into the same body while you are there.

**Guest writes:** ["mem_w32(0x1F800090, 0) — depth-cue IR0 publish. SKIPPABLE: the grid never reads it; it is state for the 0x801104D0 tail, and the guest body still executes underneath pc_render, so the tail sees it regardless.", "Math::rotmat writes 5 words to 0x1F800000; Math::matMul overwrites the same 5 words; Math::applyMatlv writes 3 words to 0x1F800014; then 3 read-modify-writes add the camera translation into 0x1F800014/18/1C. ALL SKIPPABLE — the native producer composes the same transform in float via Render::projComposeObjectHost and never touches the workspace. This is the reason the port needs a host-output rotmat: calling the existing Math::rotmat(anglesPtr, out) from pc_render would be a guest write and an SBS violation.", "Per quad, into the packet pool at *0x800BF544: the tag word, four RGB words, four SXY words, four u/v byte pairs, the code byte, the tpage halfword and the clut halfword — 52 bytes. ALL SKIPPABLE.", "Per emitted quad, the OT link: mem_w32(otslot, pkt) at *0x800ED8C8 + key*4, plus the tag's next-pointer field. SKIPPABLE.", "mem_w32(0x800BF544, pkt) at the end — the advanced pool cursor. SKIPPABLE.", "Stack frame: sp -= 88, spills of r16..r22 and ra at sp+56..sp+84, and the four 8-byte model vertices at sp+16..sp+47 plus scratch at sp+48 (FLAG) and sp+52 (otz/key). These are GUEST-STACK writes, but they belong to the substrate body that still runs; a read-only pc_render producer keeps its vertices in host locals and mirrors nothing.", "NET: a read-only native producer performs ZERO guest writes. Every write above is either transform scratch it replaces with float math, or packet/OT emission it replaces with drawWorldQuad."]


### Proposed wrappers

- struct BackdropPlaneNode — a typed lens over the type-0x20 node, so the body reads as game code: pos() -> {s16 x@0x2C, y@0x2E, z@0x30}, rot() -> {s16 vx@0x48, vy@0x4A, vz@0x4C}, seqState() -> u8@0x05. Put it in the new game/render/fx_backdrop_plane.cpp (or .h) next to the producer; do NOT open-code mem_r16(node+44).
- Named scratchpad/global constants (reuse the names already established elsewhere in game/render/ rather than inventing new ones): kFrameCounter = 0x1F80017C (field_hud.cpp), kScrSceneCamera = 0x1F8000F8 (perobj_billboard.cpp calls it CAM2), kScrDepthCue = 0x1F800090 (swing_fx.cpp), kPktPoolPtr = 0x800BF544, kOtBasePtr = 0x800ED8C8. New one this port introduces: kSeqStateWord = 0x800BF850 with a comment stating the ONLY predicate used here is (uint32)*word < 2.
- enum class PlaneHalf { Upper, Seam, Lower, LowerFirst } — or, more honestly, three named colour constants plus two named predicates. The row colouring is a 4-way fork on the outer row y: kColNeutral = 0x00808080, kColSeam = 0x00DFDFDF, kColDeep = 0x00201000, with rowIsUpper(y) = (y <= 0), rowIsSeam(y) = (y == 0), rowIsFirstBelow(y) = (y == 2400). Writing it as an if-chain over named predicates makes the mirror-about-the-seam structure visible; a transcription of the branch layout hides it.
- Named geometry constants: kCellW = 1200, kCellH = 2400, kBandDepth = 1200, kColMin = -6000, kColLimit = 6000, kRowMin = -7200, kRowLimit = 7201, kCols = 10, kRows = 7. Name kRowLimit honestly as the guest's asymmetric 7201 (it is < 7201, not <= 7200 — same set, but do not silently normalise it).
- Named texture constants: kTpage = 45 (comment: page X 832, Y 0, 4-bit CLUT, semi mode 1 = additive), kClut = 16190 (comment: CLUT at 992,252), kUHalf = 32, kUFull = 63, kVSpan = 62, and two derived-per-frame locals uBase / vBase with the derivation written out (uBase = ((tick>>1) & 3) << 6, vBase = ((tick>>1) << 4) & 192) plus a one-line comment saying U cycles every 8 ticks and V every 32.
- A shared OT-bucket helper. swing_fx.cpp already has a file-local otBucket(depth) with EXACTLY this log map and the [4, 0x7FF] range gate, and SpriteAnchor::otKeyInRange has the same map with a different pre-step. Rather than a third copy, promote one to a named static — e.g. SpriteAnchor::otBucket(int32_t otz) returning the key or -1 — and have swing_fx and this producer both call it. Grid A additionally needs the pre-snap (otz in [-39,3] -> 5), the >=2048 -> 2047 clamp, the post-map row bias and the >=2045 clamp; keep those AT THE CALL SITE with comments, because grid B has none of them and collapsing the two into one 'key' function is exactly how the ordering gets silently wrong.
- Two small private methods on Render rather than one 200-line body: backdropPlaneEmitGrid(...) for grid A and backdropPlaneEmitBand(...) for grid B, both feeding a shared quad-submit lambda. Public entry Render::fxBackdropPlaneRender(uint32_t node), declared in render.h next to fxRingSpriteRender / fxParticleFieldRender, wired in render_walk.cpp's type-0x20 chain with the overlay-residency guard c->mem_r32(0x80110CA4u) == 0x27BDFFA8u (addiu sp,-88), matching the pattern used for 0x80110C14 and 0x8010C7F4.

### Risks

- GTE FLAG CULLING IS NOT REPRODUCIBLE IN FLOAT. The guest drops a quad whenever CR31 bit31 is set after RTPT or after RTPS — that ORs in MAC1-3 overflow, SX2/SY2 saturation (screen coords outside +/-1024), SZ3 saturation and divide overflow (H/SZ3 > 0x1FFFF). On a plane 12000 x 16800 units across, near-camera and behind-camera vertices will trip this constantly, so a meaningful fraction of the 80 quads are culled by FLAG on a typical frame. EObjXform::project clamps px/py to +/-1024 and sz to [0,65535] but returns no flag. Without reproducing at least the screen-coordinate and SZ3 saturation conditions, the native producer will draw quads the guest drops — most visibly as smeared geometry at the plane's near edge. This is the single biggest fidelity risk and I could not resolve it statically; it needs a live emission-count comparison against the guest.
- ZSF4 DEPENDENCY. AVSZ4 uses CR30 (ZSF4), which this function never programs — it inherits whatever the scene frame published. proj_zsf4() returns the frame-time capture and is 0 until the first camview_publish. swing_fx.cpp handles this by skipping the gate when zsf4 <= 0; this producer must decide the same way, and 'no key captured' means the OT-range cull silently does not run.
- THE ROW BIAS IS AN AUTHORED SORT ORDER THE DEPTH BUFFER WILL NOT REPRODUCE. Grid A adds +40 to the key for rows below the seam and +30 above, AFTER the log compression, and clamps at 2045. That deliberately pushes the lower half 10 buckets further back than geometry alone would. Grid B has no such bias and no clamp. Under pc_render's per-pixel depth these two halves and the additive band sort purely by real Z, so the guest's intended layering can invert — the same class of problem docs/parity notes record for the barrel water surface. Consider passing the guest key as drawWorldQuad's sort_key (shift is 0 here, so key is the final bucket) rather than sort_key = -1.
- SMALL NUMERICAL DIVERGENCES FROM THE FLOAT COMPOSE. Math::matMul clamps every composed rotation element to int16 and truncates >>12; Math::applyMatlv clamps the rotated translation to int16 before the camera translation is added. projComposeCore does both in double with no int16 clamp. For a node position of magnitude ~9216 against a 4096-scale rotation the intermediate can reach ~27000 — inside int16, but not with margin, and any clamp the guest hits is a divergence the float path will not have. Sub-pixel in the common case; worth a one-time check of the composed T against the guest's CR5-7.
- THE VISUAL IDENTITY IS INFERRED, NOT SEEN. I am a read-only scout and did not run the game. tpage 45 / clut 16190 identify VRAM coordinates, not content. The colour ramp, the V-mirror about the seam and the additive band are read straight out of the instruction stream and are solid; what the texture actually depicts, and therefore what the effect IS, is not.
- 0x800BF850 IS ONLY PARTIALLY IDENTIFIED. It is a MAIN.EXE global written at 21 sites and this area's own tick fn stores 2 into it; another actor decrements it and a third compares it to 128. I could not establish a single authoritative meaning. This does not block the port — the producer only needs the (uint32)*word < 2 predicate — but do not name the constant something specific like 'cutsceneStage' without evidence.
- PORTING THE GRID ALONE LEAVES AREA 14 INCOMPLETE. 0x801104D0 is tail-called from here and from nowhere else, so it will not appear as a separate entry in a nofx census once 0x80110CA4 is whitelisted — the sweep will report the address as covered while its sprite half is still dark. Track it explicitly rather than relying on the census.


---

## 0x801110BC — area 11 (A0B overlay)

**Portable:** straightforward — Nothing blocks it. The 'matrix pipeline' is three trivial libgte primitives, not real work: 0x80084660 = SetRotMatrix (5 lw + 5 ctc2 into CR0-4), 0x80084690 = SetTransMatrix (3 lw + 3 ctc2 into CR5-7), 0x80084220 = ApplyMatrixLV-lite (already Math::applyMatlv, LIVE). gte_op(0x4A180001) decodes to plain RTPS with sf=1, lm=0 — the exact operation EObjXform::project already reproduces in float (0-diff vs Beetle per docs/tomba2-native-projection). The whole GTE setup is algebraically identical to 'the pure scene camera, pre-translated by the field origin', which is exactly what Render::projComposeObjectHost(Robj=4096*I, Tobj=origin, &cam) produces (projComposeCore: R = Rcam*Robj/4096, T = Rcam*Tobj/4096 + Tcam). So the port needs NO new matrix helper and NO guest-visible behavior. The only genuinely new thing versus the existing fx_* producers is the primitive kind: an untextured flat screen-space square rather than a textured record quad — and mode=3 flat prims already have precedent in this exact queue path (fx_line.cpp:172, fx_ring.cpp:161).

**Effect:** A camera-following volumetric DOT HAZE — 513 opaque pure-white 1x1/2x2 pixel specks filling a 2048-unit cube centred one cell-width ahead of the camera, wrapping infinitely on a world lattice keyed to the node's anchor, with the only depth cue being the 2x2-if-nearer-than-SZ3-1536 size step. No texture, no colour, no fade, no scale-with-distance, no rotation, no per-frame randomisation (the seed is never advanced across frames). That combination — white points, screen-space fixed size, infinite camera-relative wrap, a single flat OT bucket — is the classic PSX SNOW / STARFIELD / firefly-motes idiom, and the wrap anchor at node+0x2C/0x2E/0x30 is exactly the hook a behaviour would drive to make the specks FALL or DRIFT. My best reading is falling snow or drifting dust motes in area 11; a static star/sparkle field is the same code with a static anchor. I cannot distinguish them statically because I did not RE whatever updates node+0x2C/0x2E/0x30 — if that anchor's Y decreases each frame it is snow, if it is constant it is a starfield. One frame of `PSXPORT_DEBUG=fxdots` logging the anchor across two frames settles it.

**Summary:** overlay guest 0x801110BC is NOT a member of the FUN_80027A4C sprite-effect family: it calls no record writer, never programs DQA/DQB, never reads MAC0, and never touches 0x1F800084/88/90. It is a self-contained VOLUMETRIC DOT FIELD — a 513-point pseudo-random cloud filling a 2048-unit cube that follows the camera, emitted as raw GP0 0x60 monochrome variable-size rectangles (colour 0xFFFFFF, opaque) chained straight into the fixed OT bucket 256. The setup does the camera work by hand instead of via FUN_800329E0: FUN_80084660 loads the pure scene-camera rotation from 0x1F8000F8 into GTE CR0-4 (SetRotMatrix), FUN_80084220 (= Math::applyMatlv, MVMVA sf=1 v=V0 cv=NONE) rotates a FIELD ORIGIN vector by it, the scene camera's own view translation at 0x1F80010C/110/114 is added, and FUN_80084690 (SetTransMatrix) puts the sum in CR5-7. The net effect is exactly the scene camera pre-translated by the field origin, so each particle's LOCAL 11-bit coordinate RTPSes as if it were the world point origin+local. The field origin is camLookPos - 1024 + (camera-forward-row >> 1) per axis, i.e. the 2048 cube is centred one full cell-width ahead of the camera; particle coordinates are ((LCG >> 16) + nodeAnchor - origin) & 2047, so the cloud is anchored to a world-space 2048 lattice keyed on the node's own anchor at node+0x2C/0x2E/0x30 and wraps infinitely as the camera moves. The only gates are the GTE FLAG error bit after RTPS and an unsigned screen-X < 320 clip; SZ3 < 1536 picks a 2x2 rect, otherwise 1x1. Nothing blocks a native port: the only unowned callees (0x80084660, 0x80084690) are 10- and 6-instruction libgte CR setters that a read-only native producer does not need at all, because projComposeCamera already reads the same scratchpad matrix and projComposeObjectHost already performs the same pre-translation compose in float.


### Verifier corrections (CORRECTED)

- **[MAJOR]** claim: STEP 7e — GATE 2: "r2 = (u16)*(pool+12) — that is the LOW halfword of the SXY2 word just written"

  actual: Line 4638: `c->r[2] = (uint32_t)c->mem_r16((c->r[5] + 4))`. r5 was set at line 4591 to `r19 + 4` and is only advanced (+16) inside the emit block alongside r19, so r5+4 == r19+8 == pool+8, NOT pool+12. pool+12 is the SIZE word, which at that point in the iteration has not yet been written (it is stored at line 4649, AFTER the gate) — a literal reading of the spec would read the PREVIOUS dot's size word. The spec is self-contradictory: its own STEP 7d correctly places SXY2 at pool+8. The derived rule (unsigned low halfword of SXY2 < 320) is nevertheless correct.

- **[MINOR]** claim: STEP 2: "The guest stores each as a halfword at sp+16/+18/+20 and re-reads it sign-extended (`<<16 >>16`)"

  actual: There is no memory re-read. Lines 4529-4531 / 4536-4542 / 4540-4548 do `mem_w16(sp+N, (u16)rX)` and then `rX = (int32)(rX << 16) >> 16` on the REGISTER. The stack halfwords at sp+16/18/20 are written only as the SVECTOR argument for FUN_80084220 and are never loaded back. Numerically identical result; the mechanism description is wrong.

- **[MINOR]** claim: STEP 7e: "Because the read is unsigned, a negative SX becomes >= 0xFF00 and fails"

  actual: SX2 is GTE-saturated to [-1024, 1023], so a negative SX reads as 0xFC00..0xFFFF, not necessarily >= 0xFF00. The conclusion (effective test 0 <= SX < 320) is unaffected.

- **[MINOR]** claim: STEP 4 / calls[0x80084220]: "MVMVA ... on the origin SVECTOR" at sp+16

  actual: guest 0x80084220 (authenticated executable/overlay evidence) loads a0+0 and a0+4 as full 32-bit WORDS into GTE data 0 and 1. a0+4 = sp+20 covers originZ at sp+20 AND sp+22, which this 64-byte frame never writes (spills start at sp+24) — so VZ0's high half is uninitialised stack garbage. Harmless (VZ0 is a 16-bit register), but the spec flags the identical situation for 0x1F8000C6 in the loop and omits it here.

- **[MINOR]** claim: STEP 2 / globals: S+2/S+6/S+10 named "camLookPos" / "camera look position"

  actual: cutscene_camera.cpp:558-560 computes dX = (S+14)-(S+2), dY = (S+18)-(S+6), dZ = (S+22)-(S+10) and line 599-601 negates S+2/S+6/S+10 into VX/VY/VZ before applyMatrixLV(...,0x1F80010C). S+2/6/10 is therefore the camera EYE/FROM position; S+14/18/22 is the look-AT target. "camLookPos" reads as the target and is misleading. The geometry claim ("cube centred one cell-width ahead of the camera") is only correct because it is the eye — which it is.

- **[MINOR]** claim: risks: the S+2/S+6/S+10 axis order is left unresolved because cull.cpp:383 says "+2=X,+6=Z,+10=Y"

  actual: Resolvable statically and the spec's reading is right: lookAt builds yaw from ratan2(-dX, dZ) with horizontal length sqrt(dX^2+dZ^2) and pitch from dY against that horizontal length, so S+6 (dY) is the VERTICAL axis => S+2=X, S+6=Y, S+10=Z. game/render/cull.cpp:383's comment is the stale/wrong one. Not a spec error — a risk that should have been closed rather than left open.


### Algorithm (as CORRECTED by the verifier — a DELTA; read with the ORIGINAL below)

1. STEPS 1-6 stand as written in the spec, with two wording fixes: (a) STEP 2's sign-extension is a REGISTER sll16/sra16 on the value just stored, not a re-read from sp+16/18/20 (identical result); (b) S+2/S+6/S+10 is the camera EYE/FROM position, not the look-AT target at S+14/18/22 — rename camLookPos -> camEyePos. The axis order X,Y,Z is CONFIRMED (lookAt takes pitch from dY = (S+18)-(S+6) against the horizontal length, so S+6 is vertical); game/render/cull.cpp:383's '+2=X,+6=Z,+10=Y' comment is the stale one and should be fixed when touched.
2. STEP 4 addendum: FUN_80084220 loads a0+0 and a0+4 as WORDS, so VZ0's high half comes from sp+22, which this 64-byte frame never initialises. Harmless (VZ0 is 16-bit) but it is guest-stack garbage, not zero — do not assume a defined value there.
3. STEP 7e — GATE 2 (screen-X clip), CORRECTED ADDRESS: r5 is the invariant `pool + 4`, so the gate reads `(uint16_t)mem_r16(r5 + 4)` == `(uint16_t)mem_r16(pool + 8)` — the LOW halfword of the SXY2 word written one instruction earlier at pool+8. NOT pool+12 (that is the size word, which is written later at line 4649 and would hold the previous dot's value at gate time). `if (!((uint32_t)sx_u16 < 320)) skip`. Because the read is unsigned and SX2 saturates to [-1024,1023], a negative SX reads as 0xFC00..0xFFFF and fails; effective test is 0 <= SX < 320. There is no Y test.
4. STEPS 7f-8 and the NATIVE PORT recommendation stand as written. The port is unaffected by the pool+12 error (a native producer emits no packet), but the packet layout in any comment must read: pool+0 = OT tag, pool+4 = 0x60FFFFFF, pool+8 = SXY2 (and the source of the <320 gate), pool+12 = size word.

### Algorithm (ORIGINAL spec steps — the corrections above are stated against these)

1. ENTRY (guest): r29 -= 64; a0 = node; r17 = 0x1F8000D0 (CutsceneCamera::S); r19 = *(u32*)0x800BF544 (packet-pool bump cursor); r18 = *(u32*)(node+0x50) (the LCG seed, read but NEVER written back — the pattern is deterministic per node per frame). r23 = 0x1F800080 (scratch slot).
2. STEP 1 — the camera-forward half-step. Read the view matrix's THIRD ROW: m6=(s16)*(0x1F800104), m7=(s16)*(0x1F800106), m8=(s16)*(0x1F800108) (= S+52/54/56, the third row of the CR-packed 3x3 at S+40, i.e. the camera's forward axis expressed in WORLD coords, 4096 = 1.0). Each is transformed as (sext16(v) << 11) >> 12 arithmetic, which is EXACTLY sext16(v) >> 1 (floor) — no 32-bit truncation occurs, so the port may write `>> 1`. Call these fx, fy, fz.
3. STEP 2 — the FIELD ORIGIN. Read the camera look position as UNSIGNED halfwords: cx=(u16)*(0x1F8000D2), cy=(u16)*(0x1F8000D6), cz=(u16)*(0x1F8000DA) (= S+2/S+6/S+10; NOTE these are X,Y,Z in that order per CutsceneCamera::lookAt — see risks). Then originX = (int16_t)(uint16_t)(cx - 1024 + fx), originY = (int16_t)(uint16_t)(cy - 1024 + fy), originZ = (int16_t)(uint16_t)(cz - 1024 + fz). The guest stores each as a halfword at sp+16/+18/+20 and re-reads it sign-extended (`<<16 >>16`), so the arithmetic is mod 2^16 then sign-extended — zero- vs sign-extending cx/cy/cz is irrelevant to the result. GEOMETRIC MEANING: origin = camLook + 2048*forward - (1024,1024,1024), so the 2048-unit cube [origin, origin+2047] is centred one full cell-width AHEAD of the camera.
4. STEP 3 — the per-axis wrap deltas. dx = (s16)*(node+0x2C) - originX; dy = (s16)*(node+0x2E) - originY; dz = (s16)*(node+0x30) - originZ. Plain int32 differences (node+0x2C/0x2E/0x30 is the family's standard world anchor triple). These are what anchor the cloud to a world lattice: local = (rand + nodeAnchor - origin) & 2047, so world = origin + local == (rand + nodeAnchor) mod 2048.
5. STEP 4 — GTE setup (guest). FUN_80084660(0x1F8000F8): CR0-4 <- the 5 packed words of the scene view rotation. FUN_80084220(sp+16, 0x1F800014): MVMVA sf=1 mx=RT v=V0 cv=NONE lm=0 on the origin SVECTOR, writing IR1/IR2/IR3 as three s32 at 0x1F800014/18/1C. Then add the scene camera's own view translation: *(0x1F800014) += *(0x1F80010C); *(0x1F800018) += *(0x1F800110); *(0x1F80001C) += *(0x1F800114). FUN_80084690(0x1F800000): CR5-7 (TRX/TRY/TRZ) <- the three words at 0x1F800014/18/1C (SetTransMatrix reads only +20/+24/+28 of its argument; the rotation part of the 0x1F800000 scratch matrix is never written and never read). NET: the GTE now holds the scene camera pre-translated by the field origin. NOTE: DQA/DQB (CR27/28) and the far colour (CR21-23) are NEVER programmed by this function.
6. STEP 5 — LCG contract. A = *(u32*)0x8011C030 (A0B overlay DATA — reload it inside the loop as the guest does, or once; it cannot change mid-frame). step(): s = s*A + 1 (32-bit wraparound; the guest uses MULT lo, identical to unsigned 32-bit multiply-low). Each axis source is the ARITHMETIC shift (int32_t)s >> 16, range [-32768, 32767].
7. STEP 6 — PRE-LOOP, particle 0. x0 = (int32)s >> 16 read from the RAW SEED, BEFORE any step. step(). y0 = (int32)s >> 16. step(). z0 = (int32)s >> 16. step(). The third step's result (call it s3) is COMPUTED BUT NEVER READ — the loop steps again before its first read. Vertex written to scratchpad: VX = (x0 + dx) & 2047 -> 0x1F8000C0, VY = (y0 + dy) & 2047 -> 0x1F8000C2, VZ = (z0 + dz) & 2047 -> 0x1F8000C4 (three u16 stores).
8. STEP 7 — LOOP. r6 = 512, decremented at the BOTTOM, branch while (int32)r6 >= 0 => the body runs for r6 = 512..0 inclusive = 513 ITERATIONS, i.e. particles 0..512. The body is software-pipelined: it RTPSes the CURRENT particle's staged vertex, then computes the NEXT particle's vertex, then runs the current particle's gates/emit. The last iteration's next-vertex computation is wasted work.
9. STEP 7a — project. gte_write_data(0, *(u32*)0x1F8000C0) = VXY0 (VX low, VY high); gte_write_data(1, *(u32*)0x1F8000C4) = VZ0 (only the low 16 bits matter; 0x1F8000C6 is never written and its garbage is discarded by the GTE). gte_op(0x4A180001) = RTPS, sf=1, lm=0.
10. STEP 7b — next particle, interleaved with the RTPS. Exact order: step() [call it sA] -> issue RTPS -> nextX = ((int32)sA >> 16) + dx, & 2047, store 0x1F8000C0; step() [sB] -> nextY = ((int32)sB >> 16) + dy, & 2047, store 0x1F8000C2; step() [sC] -> nextZ = ((int32)sC >> 16) + dz, & 2047, store 0x1F8000C4. THREE steps per iteration, X from the 1st, Y from the 2nd, Z from the 3rd. Combined with STEP 6 the global read sequence is s0,s1,s2 | s4,s5,s6 | s7,s8,s9 | ... — s3 is the one skipped value.
11. STEP 7c — GATE 1 (GTE FLAG). r12 = gte_read_ctrl(31); *(u32*)0x1F800080 = r12; if ((int32)FLAG < 0) SKIP this particle (no packet). FLAG bit31 = OR of bits 30..23 and 18..13, i.e. MAC1-3 overflow, IR1-3 saturation, SZ3 saturation, divide overflow, MAC0 overflow, SX2/SY2 saturation.
12. STEP 7d — publish. *(u32*)(pool+8) = gte_read_data(14) (SXY2, packed SY<<16|SX); *(u32*)0x1F800080 = gte_read_data(19) (SZ3, overwriting the FLAG it just stored there).
13. STEP 7e — GATE 2 (screen-X clip). r2 = (u16)*(pool+12) — that is the LOW halfword of the SXY2 word just written, read UNSIGNED = SX. if (!(r2 < 320)) SKIP. Because the read is unsigned, a negative SX becomes >= 0xFF00 and fails: the effective test is 0 <= SX < 320. There is NO Y test.
14. STEP 7f — size. sz = *(u32*)0x1F800080 (SZ3). if ((int32)sz < 1536) size = 0x00020002 (2x2) else size = 0x00010001 (1x1). Packed as height<<16 | width for GP0 0x60.
15. STEP 7g — packet + OT link. *(u32*)(pool+4) = 0x60FFFFFF (GP0 0x60 monochrome variable-size rectangle, colour 0xFFFFFF, OPAQUE — not 0x62/semi). *(u32*)(pool+12) = size. otbase = *(u32*)0x800ED8C8; *(u32*)(pool+0) = *(u32*)(otbase+1024) | 0x03000000 (tag: previous head + 3-word length); *(u32*)(otbase+1024) = pool (prepend into the FIXED OT bucket index 256 for EVERY dot — no per-dot depth sorting). pool += 16.
16. STEP 8 — EXIT. *(u32*)0x800BF544 = pool (the bump cursor advanced by 16 per EMITTED dot only). Registers restored, r29 += 64. node+0x50 is NOT written back.
17. NATIVE PORT (recommended shape, read-only display-pass producer in fieldObjectsRender's type-0x20 walk): (1) compute fx/fy/fz, origin, dx/dy/dz exactly as STEPS 1-3, all from guest reads; (2) build the field camera ONCE with `const float Robj[3][3] = {{4096,0,0},{0,4096,0},{0,0,4096}}; const float Tobj[3] = {(float)originX,(float)originY,(float)originZ}; EObjXform cam; projComposeObjectHost(Robj, Tobj, &cam);` — this is algebraically the guest's SetRotMatrix + applyMatlv + SetTransMatrix, and it routes through Fps60::sceneCam so the field lerps at 60fps for free; (3) run the LCG contract of STEPS 5-7b verbatim in a plain for-loop over 513 particles, keeping the pre-loop/first-read asymmetry; (4) `ProjVtx pv; cam.project(localX, localY, localZ, &pv);` with the LOCAL 0..2047 coordinates — DO NOT pass origin+local, because EObjXform::project casts its arguments to (int16_t) and world coords can exceed s16; (5) gate as in STEP 7c/7e (see emitContract gateBias for the FLAG-equivalent tests); (6) emit one untextured flat white square via RenderQueue::emitOrQueue(c, 1, RQ_WORLD, RQ_OM_DEPTH, 4, /*semi=*/0, /*raw=*/0, xs, ys, xsf, ysf, uv, uv, w, w, w, depth, /*mode=*/3, 0,0,0,0, 0,0,0,0, 0,0,1023,511, /*blend=*/0) with corners (px,py),(px+w,py),(px,py+h),(px+w,py+h), w=h=1 or 2, colour 255,255,255, depth[k] = proj_pz_to_ord(pv.pz); (7) wrap the whole loop in ObjScope objScope(c, node) and add a cfg_dbg channel logging drawn/513 plus the screen extent (instrument 022: a 0-px A/B means 'not visible at this instant', so the producer must log its own extent).

### Node fields

| offset | type | name | meaning |
|---|---|---|---|
| `+0x18` | u32 | renderFn | the type-0x20 custom render fn — == 0x801110BC for this node. Whitelist key in render_walk.cpp's fieldObjectsRender; guard word for the overlay-residency check is *(u32*)0x801110BC == 0x27BDFFC0 (addiu sp,-64). |
| `+0x2C` | s16 | fieldAnchorX | world X the 2048 lattice is keyed on (read as (int16_t)mem_r16(node+44)). Same slot as the sprite family's kAnchorXY low half — here it is used as three SEPARATE s16 (not a packed VX|VY pair, because +0x2E is read on its own). |
| `+0x2E` | s16 | fieldAnchorY | world Y the lattice is keyed on. If the node's behaviour animates this downward each frame, the cloud falls (snow); if it is static the cloud is world-fixed and only wraps as the camera moves. |
| `+0x30` | s16 | fieldAnchorZ | world Z the lattice is keyed on. Same slot as the sprite family's kAnchorZ. |
| `+0x50` | u32 | lcgSeed | the field's LCG seed. READ ONLY — this function never writes it back, so the dot pattern is identical every frame unless some other code re-seeds it. (Contrast: the same offset is the particle ARRAY base for FUN_800281EC and the wind MAGNITUDE for FUN_8010C7F4 — the family reuses 0x50 for whatever each controller needs.) |

### Globals

| addr | type | name | meaning |
|---|---|---|---|
| `0x1F8000D0` | struct base (scratchpad) | CutsceneCamera::S | r17 = (0x1F80 << 16) + 208. The scratchpad camera scratch block already owned by game/camera/cutscene_camera.h (constant `S`). Everything below at +NN is an offset into it. |
| `0x1F8000D2` | u16 (read zero-extended) | S+2 camLookX | camera look/eye X. Written by CutsceneCamera::lookAt, which negates S+2/S+6/S+10 into VX/VY/VZ — so the order is X,Y,Z. (fx_sprite.cpp's kFieldDistRef reads the same three at +0/+4/+8.) |
| `0x1F8000D6` | u16 (read zero-extended) | S+6 camLookY | camera look/eye Y. |
| `0x1F8000DA` | u16 (read zero-extended) | S+10 camLookZ | camera look/eye Z. |
| `0x1F8000F8` | 5 x u32 (CR-packed 3x3 s16) | S+40 sceneViewRot | the composed scene view rotation, CR0-4 packing (m11,m12 | m13,m21 | m22,m23 | m31,m32 | m33). Argument to FUN_80084660. This is the SAME block Fps60::sceneCam reads for R and leaf_800329E0 loads into CR0-4. |
| `0x1F800104` | s16 | S+52 viewRot m31 | matrix element index 6 = third ROW, X component = the camera's forward axis in world coords (4096 = 1.0). Used as m31 >> 1 in the field-origin half-step. |
| `0x1F800106` | s16 | S+54 viewRot m32 | matrix element index 7 = third row, Y component. |
| `0x1F800108` | s16 | S+56 viewRot m33 | matrix element index 8 = third row, Z component. |
| `0x1F80010C` | s32 | S+60 sceneViewTransX | scene camera view translation X (raw view units = GTE CR5/TRX). Written by CutsceneCamera::lookAt via applyMatrixLV(M, -camPos, 0x1F80010C); read by Fps60::sceneCam as T[0]. |
| `0x1F800110` | s32 | S+64 sceneViewTransY | scene camera view translation Y (CR6/TRY). |
| `0x1F800114` | s32 | S+68 sceneViewTransZ | scene camera view translation Z (CR7/TRZ). |
| `0x1F800000` | MATRIX (scratchpad) | scratchMatrix | r16 = 0x1F80 << 16. Passed to FUN_80084690, which reads ONLY +20/+24/+28. Its rotation part (+0..+17) is neither written nor read by this function. |
| `0x1F800014` | s32 x3 | scratchMatrix+20/+24/+28 composedTrans | WRITE: destination of FUN_80084220 (IR1/IR2/IR3 of R*origin), then += the scene view translation, then loaded into CR5-7. Pure guest scratch — a native producer folds this into the float T'. |
| `0x1F8000C0` | u16 x3 (VXY0 | VZ0 staging) | particleVertex | WRITE: the current particle's local 11-bit coordinates, read back as two words into GTE data regs 0 and 1. 0x1F8000C6 is never written (garbage in the high half of VZ0, ignored by the GTE). |
| `0x1F800080` | s32 | gateScratch | WRITE: first the GTE FLAG (CR31) for the error test, then overwritten with SZ3 for the size test. This is the same slot the sprite family uses to publish its OT key (leaf_800317CC) — here it is pure scratch, and no OT key is ever computed. |
| `0x800BF544` | u32 | packetPoolCursor | MAIN RAM (MAIN.EXE global; (32780 << 16) - 2748). The shared packet-pool bump-allocator write pointer — same constant as game/render/widescreen_margin_quad.cpp kPktPoolPtr, text_label.cpp PKT_POOL_PTR etc. Read at entry, advanced 16 bytes per EMITTED dot, written back at exit. |
| `0x800ED8C8` | u32 -> OT base | otBasePtr | MAIN RAM ((32783 << 16) - 10040). Dereferenced to the live ordering-table base; the field links every dot into base+1024, i.e. OT INDEX 256, a constant bucket for all 513 dots. Same global as render_walk.cpp OTBASE_PTR. |
| `0x8011C030` | u32 (constant) | fieldLcgMultiplier | A0B OVERLAY DATA, not main RAM ((32786 << 16) - 16336). It lies in the code gap 0x8011A4FC..0x80121AA4 between the last A0B function at 0x8011A4EC and the next at 0x80121AA4, i.e. the overlay's data segment. Must be read at runtime with c->mem_r32 while A0B is resident — exactly like fx_sprite.cpp's kFieldLcgMult = 0x80115894 for A0L. Its value is NOT knowable from static source. |
| `0x60FFFFFF` | literal | GP0 rect command word | (24831 << 16) | 65535. GP0 0x60 = monochrome variable-size rectangle, OPAQUE (0x62 would be the semi-transparent variant), colour 0xFFFFFF pure white. |
| `0x03000000` | literal | OT tag length field | 768 << 16. Three payload words follow the tag = the 4-word packet {tag, 0x60FFFFFF, SXY2, size}. |

### Callees

| addr | role | native equivalent |
|---|---|---|
| `0x80084660` | libgte SetRotMatrix — loads the 5 CR-packed words at a0 (= 0x1F8000F8, the scene view rotation) into GTE CR0-4. Called once, before the origin rotate. | NONE (codemap: no native owner) — and NOT NEEDED. guest 0x80084660 (authenticated executable/overlay evidence) is 10 instructions: 5 lw + 5 ctc2, pure. The native producer never programs the GTE; Fps60::sceneCam already reads the identical 5 words at SCR+0xF8 into EObjXform::R. Port it only if some other caller needs a native SetRotMatrix; it is a 5-line pure helper if so. |
| `0x80084220` | libgte ApplyMatrixLV-lite — loads the SVECTOR at a0 (sp+16, the field origin) into VXY0/VZ0, runs gte_op(0x4A486012) = MVMVA sf=1 mx=RT v=V0 cv=NONE lm=0, writes IR1/IR2/IR3 as three s32 at a1 (0x1F800014). Rotates the field origin by the view matrix. | Math::applyMatlv (LIVE, game/math/gte_math.cpp:351, wired into the override registry by Math::registerOverrides). The native producer does not call it either — projComposeCore's `T = (Rcam . Tobj)/4096 + Tcam` is the same computation in float. |
| `0x80084690` | libgte SetTransMatrix — loads the three s32 at a0+20/+24/+28 (= 0x1F800014/18/1C) into GTE CR5/CR6/CR7 (TRX/TRY/TRZ). | NONE (codemap: no native owner) — and NOT NEEDED. guest 0x80084690 (authenticated executable/overlay evidence) is 6 instructions: 3 lw + 3 ctc2, pure. Subsumed by projComposeObjectHost's T composition. |
| `gte_op(0x4A180001)` | RTPS. Decoded: COP2 (op=0x12), imm25=0x0180001, opcode field 0x01 = RTPS, sf=1 (shift 12), mx/v/cv irrelevant for RTPS, lm=0. Perspective-transforms V0 (the staged local particle coordinate) and produces SXY2 (data reg 14), SZ3 (data reg 19) and FLAG (ctrl 31). | EObjXform::project (game/render/projection.cpp:85) — the float reimplementation of RTPS used by every native producer, 0-diff against Beetle on 1.28M verts (memory tomba2-native-projection). The only piece it does not surface is the FLAG error bit; see emitContract.gateBias for the reconstruction. |

### Emit contract

- **writer:** NO record writer — this emitter is NOT in the FUN_80027A4C / FUN_8002847C families. It builds its own 4-word GP0 packet inline at the shared pool tail: {tag, 0x60FFFFFF, SXY2, size}, where 0x60 = monochrome variable-size RECTANGLE, colour 0xFFFFFF, OPAQUE (no semi bit). Native equivalent: one untextured flat quad per dot through RenderQueue::emitOrQueue(RQ_WORLD, RQ_OM_DEPTH, nv=4, semi=0, mode=3), the same call shape as fx_line.cpp:172 / fx_ring.cpp:161. Corners are the axis-aligned square (px,py)-(px+w,py+h); GP0 rects cover [x,x+w) x [y,y+h).
- **gateBias:** NO OT-key bias gate and NO call to FUN_800317CC — SpriteAnchor::otKeyInRange does NOT apply here. Two gates, both per-dot: (1) GTE FLAG (CR31) bit31 set after RTPS -> skip; (2) the LOW halfword of SXY2 read as UNSIGNED < 320 -> so effectively 0 <= SX < 320, a hard clip to the 320-wide framebuffer, with NO Y test at all. Native reconstruction of gate (1), in order: skip if pv.sz <= 0 (SZ3 saturated / behind camera, FLAG bit18); skip if ((int64)H << 17) > (int64)0x1FFFF * pv.sz (divide overflow, FLAG bit17 — the GTE saturates H*0x20000/SZ3 at 0x1FFFF, so anything nearer than ~SZ3 = 2H is rejected by the guest; this test MUST be explicit because EObjXform::project silently clamps pz to H/2 instead); skip if the UNCLAMPED screen coordinate leaves +/-1024 (FLAG bits14/13 SX2/SY2 saturation — recompute it as ofx + pv.vx * (H/pv.pz) and ofy + pv.vy * (H/pv.pz), because project() clamps px/py to [-1024,1023] and would otherwise draw a dot pinned at the screen edge that the guest drops); skip if |pv.vx|,|pv.vy|,|pv.vz| hit 32767 (IR saturation, FLAG bits24..22). Then gate (2): skip unless 0 <= pv.sx < 320.
- **dqa:** NOT PROGRAMMED. This emitter never writes CR27/CR28 and never reads MAC0 (data reg 24). There is no depth-cue-as-scale trick here at all; SpriteAnchor::baseScale is not used. Whatever DQA/DQB a previous emitter left in the GTE is inherited but never consumed (see risks for the one way it could still matter).
- **scaleFormula:** NO per-Z scale. The dot is a screen-space square whose side is chosen by one depth threshold: side = 2 px if SZ3 < 1536, else 1 px. The size word is packed height<<16 | width, so 0x00020002 or 0x00010001. Position is SXY2 verbatim (integer screen coords); a native producer should use the float pv.px/pv.py for sub-pixel smoothness but MUST run the <320 gate on the rounded integer sx so the accepted set matches.
- **ir0Formula:** n/a. 0x1F800090 (the family's IR0 depth-cue slot) is never touched and no DPCS is ever run — there is no record writer to apply a cue to. SpriteAnchor::depthCue is not used.
- **farColour:** n/a. CR21-23 are never written. The dot colour is the hard literal 0xFFFFFF in the GP0 command word — pure opaque white, identical for every dot, with no fade, tint or distance attenuation.
- **recordListFormula:** n/a — no record list, no clut, no tpage, no texture. Storage instead: each emitted dot takes 16 bytes from the shared bump pool at *(0x800BF544) and is PREPENDED into the single fixed bucket *(0x800ED8C8) + 1024 (OT index 256) — the same bucket for all 513 dots, so the guest gives them NO relative depth sorting. A native producer should ignore that and use real per-vertex depth = proj_pz_to_ord(pv.pz) per the engine-owns-ordering directive; keep the constant-bucket behaviour in mind only if the dots start occluding wrongly against terrain.
- **perFrameCount:** 513 candidate dots per node per frame (the counter runs r6 = 512 down to 0 inclusive, decrement-then-branch-if-non-negative). Each dot that passes both gates emits exactly ONE 4-word rect, so the upper bound is 513 primitives and 8208 pool bytes per node per frame. The last loop iteration computes a 514th particle position that is never projected.

### Helpers still needed

- `0x80084660` (10 instructions (authenticated executable/overlay evidence)): libgte SetRotMatrix (5 lw from a0+0..+16, 5 ctc2 into CR0-4). Small PURE helper, portable in minutes as Math::setRotMatrix next to the existing 8 Math methods — but the dot-field port DOES NOT NEED IT, because projComposeCamera/Fps60::sceneCam already read the identical 5 words at 0x1F8000F8. Port it only as general Math coverage, not as a dependency of this producer.
- `0x80084690` (6 instructions (authenticated executable/overlay evidence)): libgte SetTransMatrix (3 lw from a0+20/+24/+28, 3 ctc2 into CR5-7). Small PURE helper. Again NOT NEEDED by this port — the composed translation is produced in float by projComposeObjectHost.
- `n/a (host-side)` (~15 lines): OPTIONAL: a shared `Render::drawFlatScreenSquare(float px, float py, float side, float ord, unsigned char rgb[3])` helper, since fx_line.cpp, fx_ring.cpp, fx_dust.cpp and this producer all hand-roll the same mode=3 untextured emitOrQueue call with slightly different corner construction. Not required to land the port; worth doing if a fourth flat-prim producer appears.

**Guest writes:** "Every guest write this body performs is scratch or emission, and a READ-ONLY native producer can skip ALL of them. (1) Its own stack frame: r29 -= 64, callee-save spills of r16-r23/r31 at sp+24..+56, and the three-halfword origin SVECTOR at sp+16/+18/+20. A display-pass producer dispatched from fieldObjectsRender is not an override and never touches the guest stack — the guest still executes 0x801110BC itself on the faithful path, so guest-stack residency is not this port's problem. (2) Scratchpad 0x1F800014/18/1C (the composed translation), 0x1F8000C0/C2/C4 (the per-particle vertex staging) and 0x1F800080 (FLAG, then SZ3) — all pure temporaries, replaced by host locals. (3) GTE control regs CR0-4 and CR5-7 via the two libgte setters, and the GTE data/flag regs via RTPS — the native path uses float math and touches no GTE state. (4) The emission itself: 16 bytes per accepted dot written at the packet-pool cursor, the OT head at *(0x800ED8C8)+1024 relinked per dot, and the cursor at 0x800BF544 written back at exit — all replaced by RenderQueue::emitOrQueue, which writes only host memory. NOTE the pc_render invariant holds trivially here because the guest's own execution of this function (on the faithful/oracle leg) still performs every one of these writes; the native producer merely draws the same picture from the same state without repeating them. Nothing in this body writes game-logic state — not even the LCG seed at node+0x50, which is read and never stored back."


### Proposed wrappers

- `namespace { ... }` constants in a NEW file game/render/fx_dotfield.cpp (this is not a sprite-family member, so it does not belong in fx_sprite.cpp): kDotNodeAnchorX = 0x2Cu, kDotNodeAnchorY = 0x2Eu, kDotNodeAnchorZ = 0x30u, kDotNodeSeed = 0x50u — node-field offsets, named for what they mean here rather than reused from the sprite family's kAnchorXY (which is a PACKED pair; these are three separate s16).
- kDotLcgMult = 0x8011C030u with the comment 'A0B overlay DATA (code gap 0x8011A4FC..0x80121AA4) — valid only while A0B is resident', mirroring fx_sprite.cpp's kFieldLcgMult = 0x80115894u for A0L.
- kDotCount = 513, kDotCellMask = 0x7FFu (the 2048-unit cell), kDotHalfCell = 1024, kDotNearSz = 1536 (the 2x2/1x1 threshold), kDotScreenClipX = 320 (the emitter's own unsigned SX clip), kDotColour = 255 — every literal in the body named for what it IS.
- A `struct SceneCamBlock` typed lens over CutsceneCamera::S so the origin build reads as game code instead of scratchpad hex: lookX()/lookY()/lookZ() over S+2/S+6/S+10 and fwdX()/fwdY()/fwdZ() over S+52/54/56 with the comment 'third ROW of the CR-packed view matrix = the camera forward axis in world coords, 4096 = 1.0'. Best placed next to CutsceneCamera::S (cutscene_camera.h already owns the block and already names MASTER_X/Y/Z the same way) so a second reader cannot drift from the first.
- A tiny value type `struct FieldLcg { uint32_t s; uint32_t mult; int32_t axis() { const int32_t v = (int32_t)s >> 16; return v; } void step() { s = s * mult + 1u; } }` so the pre-loop's read-before-step and the loop's step-before-read asymmetry is expressed once and visibly, rather than as three inline shift/multiply pairs that a future reader will 'tidy' into the wrong order.
- A named local `fieldOrigin` (three int16_t) with the one-line geometric comment 'camera look point + 2048*forward - 1024 per axis => the 2048-unit cell is centred one full cell ahead of the camera', and `wrapDelta` (three int32_t) with 'node anchor minus origin: what turns a camera-relative cell into a world-anchored lattice'. These two names are what make the &2047 legible.
- `Render::fxDotFieldRender(uint32_t node)` declared in render.h next to fxParticleFieldRender, with a header comment stating plainly that it is NOT a FUN_80027A4C-family member (no writer, no DQA, no OT-key gate) so nobody tries to fold it into fxSpriteEmit's switch.
- A `cfg_dbg("fxdots")` channel logging node, origin, anchor, drawn/513 and the emitted SCREEN EXTENT — a single-instant A/B screenshot has been measured to report '0 px changed' for a working producer whose output is off-frame, so the extent line is the thing that makes the pixel verification falsifiable.

### Risks

- S+2/S+6/S+10 axis order: I read these as X,Y,Z on the authority of CutsceneCamera::lookAt (cutscene_camera.cpp:558-560 computes dX from S+14-S+2, dY from S+18-S+6, dZ from S+22-S+10, and cutscene_camera.cpp:599-601 negates S+2/S+6/S+10 into 0x1F8000C0/C2/C4 = VX/VY/VZ). BUT game/render/cull.cpp:383 carries the comment 'Camera pos @0x1F8000D0 (+2=X,+6=Z,+10=Y)' which contradicts it. One of the two is stale. The pairing in THIS function is symmetric (S+2 pairs with matrix element 6 and with node+0x2C; S+6 with element 7 and node+0x2E; S+10 with element 8 and node+0x30), so the port is correct either way as long as it keeps the pairing — but the cull.cpp comment should be checked and fixed or the lookAt reading confirmed, because a future reader will trip over it.
- EObjXform::project casts vx/vy/vz to (int16_t). Passing origin+local as a world coordinate WILL silently wrap for a field origin near the s16 limits. The port must pass the LOCAL 0..2047 coordinate and carry the origin in the composed translation (projComposeObjectHost with Robj = 4096*identity). Getting this backwards produces a field that looks right most of the time and teleports occasionally — the worst failure mode.
- The guest's origin rotate goes through MVMVA and reads IR1/IR2/IR3, which SATURATE at +/-32767 and truncate the >>12 toward -inf. The float compose does neither. For a 2048-cell near the camera the products stay well inside +/-32767, so this should never fire — but it is an exact behavioural difference, and if the camera is ever far from the world origin the guest would clamp where the port would not.
- The FLAG gate is the one part of this function that EObjXform::project does not expose. My reconstruction (SZ3<=0, divide overflow, SX2/SY2 saturation, IR saturation) covers every bit RTPS can raise, but I could not verify it against a running frame — this is instruction-level analysis only. The divide-overflow threshold in particular depends on whether the GTE saturates the intermediate H*0x20000/SZ3 at 0x1FFFF (which is what SpriteAnchor::baseScale in this repo assumes, giving 'reject nearer than SZ3 = 2H') or the final halved result (giving 'reject nearer than SZ3 = H/2'). The repo's own baseScale and project() disagree on this — baseScale clamps the intermediate, project() clamps pz at H/2. Whichever is right, an explicit near-reject is needed; if the dot count looks wrong on a real frame, this is the first thing to re-measure.
- The GTE's DQA/DQB are inherited from whatever emitter ran before this one in the frame. MAC0 overflow is inside FLAG bit31's OR (bits 16/15), so a pathological stale DQA could in principle make RTPS raise the error flag and suppress dots. With the family's DQA of 4 or 6 and DQB = 0 the MAC0 magnitude stays around 786k, far from overflow, so this is a theoretical coupling — but it is a real one, and it is invisible to a native producer that has no GTE state at all.
- All 513 dots go into the single OT bucket 256 in the guest, i.e. the guest gives them no depth sorting relative to each other or (precisely) to the scene. Drawing them with real per-vertex depth is the correct engine behaviour and is what every other native producer does, but it is a deliberate divergence: dots that the guest paints over a wall will instead be occluded by it. If the field looks sparser than the reference, this is why — and it is a design call, not a bug.
- 0x8011C030 is A0B overlay data whose VALUE cannot be read statically. I established it is a data word in the overlay's code gap (0x8011A4EC is the last function below it, 0x80121AA4 the first above) and that it is used as an LCG multiplier, but I have not seen the constant. The port must read it at runtime; if the field comes out degenerate (all dots at one point), dump that word first.
- The seed at node+0x50 is never written back by this function, so the pattern is frame-invariant unless the node's own behaviour re-seeds it. I did not RE the node's updater, so I cannot say whether the field ANIMATES (falling snow) or is a static world-anchored speckle that only parallaxes with the camera. The visual identity below is therefore an inference from geometry, not an observation.
- No install site for 0x801110BC exists in the A0B code (grep for the 0x8011/0x10BC constant pair finds nothing), so the pointer is copied out of a data table into node+0x18 — the same limitation `docs/issues/` records for 0x8013B118 and 0x8010C1D8. This means static tooling cannot confirm which node carries it; the whitelist entry needs the runtime residency guard (*(u32*)0x801110BC == 0x27BDFFC0) exactly like the 0x80110C14 and 0x8010C7F4 entries.
- This producer is dispatched from the display pass (fieldObjectsRender), so it must pass xsf/ysf floats (has_xyf = 1) to be re-rendered by tier1Render at the interp present — the opposite of swing_fx.cpp's guest-execution-time prims, which must pass nullptr. Getting that backwards produces the exact failure swing_fx.cpp's banner documents: prims submitted every frame and not one pixel changed.


---

## 0x801113B4 — area 3 (A03 overlay)

**Portable:** straightforward — No GTE, no projection, no camera — the geometry is already screen-space, the only maths are Trig::ratan2/rcos/rsin (all three already native and byte-verified) plus integer shifts. Every callee has a native owner or is irrelevant to a read-only producer. The four quads per segment map 1:1 onto RenderQueue::emitOrQueue (nv=4, untextured mode=3, raw=0, semi=1, tp_blend=1), exactly the shape game/render/fx_line.cpp::strokeSegment already uses. Every literal address resolves to a known, already-named global (0x800BF544, 0x800ED8C8, 0x1F800135) or to an overlay data table that is simply read at runtime. The only real judgement calls are the RQ layer/order-mode mapping for OT bucket 4 and whether to keep the guest's integer quantisation of the perpendicular.

**Effect:** A glowing, additive SCREEN-SPACE MOTION TRAIL / STREAK — the kind used for a weapon swipe, a dash afterimage or a thrown-object tracer. The evidence: the node keeps an 11-slot screen-position HISTORY at node+0x3C whose unfilled entries are (0,0) and are skipped (a ring being filled over time), consecutive entries are joined by a ribbon, and an 11-entry colour ramp fades the ribbon along its length from head to tail. Each joint is drawn as a bright ~2 px core (gouraud from the ramp colour on the centre line to BLACK at the edge) sitting inside a 8 px halo of the SAME gradient at half brightness — the textbook two-tier glow build. The whole thing is armed with GP0 semi-transparency mode 1 (100%B + 100%F, additive) by its own DR_TPAGE and linked into OT bucket 4, i.e. in front of essentially the entire 3D scene. Constant width along the trail (only the ramp tapers), and consecutive quads share an exact edge via the previous segment's perpendicular, so it reads as one continuous glowing ribbon rather than a chain of separate blobs. It sits in area 3 and, per claim C012, currently draws NOTHING under pc_render.

**Summary:** FUN_801113B4 is an 11-line wrapper: `FUN_80110B00(4, node + 0x3C, 16)`. **Both scalar arguments are DEAD in this overlay's body** — a0 (r4) is overwritten in the prologue (`r4 = 0x80108FDC + 64`, the memcpy end bound) and a2 (r6) is written (`r6 = r5 << 2`) before ever being read. Only a1 (node+0x3C) is used. The values 4 and 16 match two hardcoded constants in the body (4 quads per segment, a 16-word colour table), so they are almost certainly vestigial parameters of a shared source signature this build const-folded. This is NOT a member of the sprite-effect family: it never touches the GTE, never projects anything, and never calls FUN_800329E0/FUN_800317CC/FUN_80027A4C/FUN_8002847C. It is a pure **2-D screen-space additive ribbon/streak** emitter. It reads 11 consecutive {s16 x, s16 y} SCREEN points from node+0x3C (a position history; (0,0) = unfilled slot), walks the 10 consecutive segments, and for each non-degenerate segment writes FOUR 36-byte POLY_G4 semi-transparent quads (GP0 cmd 0x3A) straight into the shared packet pool and links them into OT bucket 4. Two quads are the ±(1×) perpendicular "core" (ramp colour on the centre line, BLACK at the edge) and two are the ±(4×) perpendicular "halo" at HALF brightness. After the loop it appends one 12-byte DR_TPAGE packet (via FUN_80083DE0(pool, 0, 1, 53, 0)) that selects tpage 0x35 with dither and **semi-transparency mode 1 = additive (B + F)**, linked LAST so it is drawn FIRST. Colours come from an 11-of-16-word table copied from overlay data at 0x80108FDC. Everything the guest writes is packet-pool / OT / pool-cursor state, so a READ-ONLY native producer can reproduce the picture with zero guest writes.


### Verifier corrections (CONFIRMED)

- **[MINOR]** claim: calls[0x80085690].nativeEquivalent: "Trig::ratan2 ... Already an installed, MIRROR_VERIFY'd override." and portable.reason: "Trig::ratan2/rcos/rsin (all three already native and byte-verified)".

  actual: game/math/trig.cpp:114 is `void Trig::registerOverrides(Game* /*game*/) {}` — an EMPTY body. The banner immediately above (trig.cpp:105-113) states the opposite of the spec: "UNREGISTERED (2026-07-15): rsin/ratan2 are NOT safe as overrides. Their substrate bodies (guest 0x80083E80 / 80085690) descend a guest STACK FRAME ... under SBS-full the guest-stack bytes diverge ... AUTO_SKIP SBS-full catches it at f560; the earlier dark-screen MIRROR_VERIFY gate did NOT (it doesn't compare that dead-stack region — an under-gating bug)... Do NOT re-register without full guest-stack-frame mirroring." So NONE of rsin/rcos/ratan2 is an installed override, and MIRROR_VERIFY is explicitly documented as having FAILED to gate them. The actionable guidance is still right (a read-only producer calls the Trig methods DIRECTLY, which is exactly the sanctioned use — "the Trig methods remain for DIRECT callers"), but the provenance label is false and must not be copied into the new file's banner.

- **[MINOR]** claim: guestWrites: "(3) OT BUCKET: *(*(0x800ED8C8) + 16) is rewritten five times per emitted segment plus once for the mode packet".

  actual: FOUR times per emitted segment, not five. The gen does exactly four head stores in the emit block (ov_a03_shard_1.c, the block at gen-relative lines 210/216/220/223 of overlay guest 0x80110B00): mem_w32(r3+16, r22)=P0, mem_w32(r4+16, r14)=P1, mem_w32(r4+16, r3=P2), mem_w32(r4+16, r17=P3). The spec's own `algorithm` OT-LINK step says "four separate head updates" and is correct; `guestWrites` contradicts it. (Total per emitted segment = 4 tag words written into the packets + 4 head writes; plus 1 tag + 1 head write for the DR_TPAGE.)

- **[MINOR]** claim: algorithm/INIT: "Then four redundant halfword stores of (s2,s3) into P+24/P+26 and P+60/P+62 — DEAD, the first emitted segment overwrites them".

  actual: They are overwritten only if at least one segment emits. If every point is degenerate (s7 never 0), no emit block runs, r22 never advances, and those four halfwords survive at P+24/26/60/62 — which is past the 12-byte DR_TPAGE the function then lays down at the same P. Harmless for the picture and for a read-only producer, but the blanket "DEAD" is false for a byte-exact override, where those stores must still be reproduced. (Also note they are stored regardless of the branch: mem_w16(r22+26) sits in the delay slot of the pts[0].x!=0 test.)


### Algorithm

1. ENTRY (wrapper 0x801113B4): sp -= 24, spill ra at sp+16, set a0 = 4 (DEAD), a1 = node + 0x3C, a2 = 16 (DEAD), call 0x80110B00, restore, sp += 24. The port takes `node` and needs only node+0x3C.
2. PROLOGUE (0x80110B00): sp -= 152; spill s0..s7/fp/ra at sp+112..sp+148. s7 (the degenerate-history flag) = 0. s6 = *(u32*)0x800BF544 = the packet-pool bump cursor (stored KUSEG-masked, i.e. 0x000Bxxxx..0x000Exxxx — this is why OR-ing a length byte into it produces a valid OT tag). Call this P.
3. COLOUR-TABLE COPY: memcpy 64 bytes from 0x80108FDC (16 u32 words) to sp+24..sp+87, four 16-byte iterations. This is the per-point colour ramp, table[j] at 0x80108FDC + 4*j. Only table[0..10] are ever read (11 of 16). A native producer reads them directly from 0x80108FDC — no copy needed.
4. POOL-ROOM GATE: parity = *(u8*)0x1F800135 (scratchpad frame/double-buffer parity, the same byte hud_gauge_emitter.cpp reads as hudViewportY). poolEnd = (parity == 0) ? 0x800D3E68 : 0x800E7E68. limit = (poolEnd & 0x00FFFFFF) - 4632. If NOT (unsigned) P < limit -> RETURN IMMEDIATELY, writing nothing at all (not even the pool cursor). fp (prevPerpX) = 0 is set in the branch delay slot. A read-only native producer does NOT need this gate (it allocates nothing) — but reproducing it keeps the picture in sync with the guest on a pool-exhaustion frame; low risk either way, the guard is generous (a full run is 1452 bytes vs 4632 headroom).
5. INIT: *(u32*)(sp+88) = 0 -> prevPerpY = 0. s2 = (s16)pts[0].x = mem_r16s(node+0x3C+0); s3 = (s16)pts[0].y = mem_r16s(node+0x3C+2). Then four redundant halfword stores of (s2,s3) into P+24/P+26 and P+60/P+62 — DEAD, the first emitted segment overwrites them; skip in the port.
6. INIT (cont.): if (pts[0].x == 0 && pts[0].y == 0) s7 = 1. Loop counter i = 1. ptsPtr = node + 0x3C + 4 (i.e. &pts[1]). colour cursor r9 = sp+28 (i.e. &table[1]). s1 = P + 108. sp+100 = P + 36. r10 = 0x08000000 (OT tag length field: 8 data words).
7. LOOP, 10 iterations, i = 1..10 (condition after increment is `i < 11`). Per iteration read cur = pts[i] = {(s16)mem_r16(ptsPtr), (s16)mem_r16(ptsPtr+2)} into s4/s5; prev is s2/s3 (carried from the previous iteration).
8. DEGENERACY HISTORY (exact order matters): s7 = ((s7 << 1) & 3); then if (cur.x == 0 && cur.y == 0) s7 |= 1; then if (cur.x == prev.x && cur.y == prev.y) s7 |= 1. So bit0 = 'this point is a null/duplicate', bit1 = the PREVIOUS iteration's bit0. The segment is emitted only when s7 == 0, i.e. a null/duplicate point suppresses BOTH the segment ending at it AND the next segment.
9. PERPENDICULAR (computed EVERY iteration, BEFORE the skip test, so a skipped segment still advances the joint state): dy = cur.y - prev.y; dx = cur.x - prev.x; ang = Trig::ratan2(dy, dx)  [a0=dy, a1=dx]; a = ang + 1024 (a quarter turn; 4096 = full circle). perpX = (Trig::rcos(a) * 2 + 2048) >> 12  [ARITHMETIC shift]; perpY = (Trig::rsin(a) * 2 + 2048) >> 12. Both land in {-2,-1,0,1,2} and are never both zero (|component| >= 1 at 45 degrees). Note the order: rcos is called first, its result is folded into perpX in the DELAY SLOT of the rsin jal, and rsin's argument is the ORIGINAL `a` (reloaded before perpX clobbers the register).
10. WIDE PERPENDICULAR: perpX4 = perpX << 2; perpY4 = perpY << 2; prevPerpX4 = prevPerpX << 2; prevPerpY4 = prevPerpY << 2. (Shift AFTER the >>12 rounding — the halo is exactly 4x the rounded core, not a separately rounded value.)
11. SKIP TEST: if (s7 != 0) jump straight to the loop tail (no packets, no pool advance).
12. EMIT — colours. colA = table[i-1] (= *(r9-4)), colB = table[i] (= *(r9)). Half-brightness variant: half(w) = (w >> 1) & 0xFF7F7F7F — a logical 32-bit shift then a per-byte 0x7F mask, i.e. each RGB byte independently halved with truncation, no carry between bytes.
13. EMIT — the four packets. Packet k has base Pk = P + 36*k, layout {+0 tag, +4 c0, +8 v0, +12 c1, +16 v1, +20 c2, +24 v2, +28 c3, +32 v3}; each vertex word is {lo16 = x, hi16 = y} written as two halfwords. In all four: c0 = c1 = 0x00000000 (BLACK, the outer edge), v2 = prev (the centre line), v3 = cur (the centre line). The command byte 0x3A (POLY_G4, semi-transparent) is written as a u8 at Pk+7.
14. EMIT — packet 0 (P+0): the -1x core.  v0 = prev - perpJoint, v1 = cur - perp,  c2 = colA, c3 = colB.
15. EMIT — packet 1 (P+36): the +1x core. v0 = prev + perpJoint, v1 = cur + perp,  c2 = colA, c3 = colB.
16. EMIT — packet 2 (P+72): the -4x halo. v0 = prev - perpJoint4, v1 = cur - perp4, c2 = half(colA), c3 = half(colB).
17. EMIT — packet 3 (P+108): the +4x halo. v0 = prev + perpJoint4, v1 = cur + perp4, c2 = half(colA), c3 = half(colB).
18. EMIT — the JOINT rule (this is the i==1 special case): perpJoint = (i == 1) ? perp : prevPerp, and perpJoint4 = (i == 1) ? perp4 : prevPerp4. The CUR end always uses the current segment's perpendicular; the PREV end uses the PREVIOUS segment's perpendicular so consecutive quads share an exact edge. The test is on the LOOP COUNTER i, not on 'first emitted segment' — if segment 1 was skipped, segment 2 still uses segment 1's (computed) perpendicular.
19. EMIT — all vertex arithmetic is 16-bit: every result is stored with mem_w16, so wrap in uint16_t and read back signed if you want byte-identical geometry.
20. OT LINK (per segment, four separate head updates, ctx = *(u32*)0x800ED8C8, bucket word at ctx+16 = OT index 4): P0.tag = *(ctx+16) | 0x08000000; *(ctx+16) = P0.  Then P1.tag = *(ctx+16) | 0x08000000; *(ctx+16) = P1.  Then P2.tag = P1 | 0x08000000; *(ctx+16) = P2.  Then P3.tag = P2 | 0x08000000; *(ctx+16) = P3. Net list order for one segment (head first = drawn first): P3 (+halo), P2 (-halo), P1 (+core), P0 (-core). Because each segment prepends, SEGMENTS draw in reverse order (segment 10 first). With additive blending the order is visually irrelevant.
21. ADVANCE: s1 += 144, sp+100 += 144, P += 144 (four 36-byte packets).
22. LOOP TAIL (runs for skipped segments too): prevPerpX = perpX; prevPerpY = perpY; prev = cur; ptsPtr += 4; colour cursor r9 += 4; i += 1; loop while i < 11.
23. AFTER THE LOOP: FUN_80083DE0(a0 = P, a1 = 0, a2 = 1, a3 = 53, stack arg4 = 0). Per guest 0x80083DE0 this writes: (u8)P+3 = 2; (u32)P+4 = 0xE1000000 | 0x200 (a2!=0) | (a3 & 0x9FF = 53) = 0xE1000235; (u32)P+8 = 0 (arg4 == 0, no DR_TWIN). Decoding 0xE1000235: tpage X base = 5, tpage Y = 1, SEMI-TRANSPARENCY MODE = (0x35>>5)&3 = 1 = 100%B + 100%F = ADDITIVE, colour depth 4bpp, dither on. The quads are untextured so only the blend mode matters to a native producer.
24. AFTER (cont.): link the mode packet at the HEAD with length 2 — *(u32*)P = *(ctx+16) | 0x02000000; *(ctx+16) = P; P += 12; *(u32*)0x800BF544 = P.  Because it is linked last it is drawn FIRST, arming additive blending for everything behind it in bucket 4.
25. EPILOGUE: restore s0..s7/fp/ra, sp += 152, return (no meaningful v0).

### Node fields

| offset | type | name | meaning |
|---|---|---|---|
| `+0x01` | u8 | visible | render_walk's per-frame visibility marker — the walk already gates on it |
| `+0x0B` | u8 | nodeType | 0x20 = custom-render-fn node |
| `+0x18` | u32 | renderFn | = 0x801113B4; the whitelist key. Overlay-resident, so the dispatch must also check mem_r32(0x801113B4) == 0x27BDFFE8 (addiu sp,-24) exactly like the other overlay producers in render_walk.cpp |
| `+0x3C .. +0x67` | 11 x {s16 x, s16 y} | trailPoints[11] | THE ONLY DATA THIS FUNCTION READS. Screen-space points (they go straight into GP0 XY halfwords, never projected). (0,0) means 'slot not yet filled' and suppresses two segments. Stride 4, index 0 = the oldest/head end (it pairs with table[0]). |

### Globals

| addr | type | name | meaning |
|---|---|---|---|
| `0x800BF544` | u32 | kPktPoolPtr | MAIN.EXE global (0x800C0000 - 2748). Shared packet-pool bump cursor, stored KUSEG-MASKED (0x000Bxxxx..0x000Exxxx) so a length byte can be OR-ed into it to form an OT tag. Already named in game/render/submit.cpp, tile_grid_layer.cpp, overlay_gt3gt4.cpp. Read at entry, written once at exit. A read-only producer touches neither. |
| `0x1F800135` | u8 | kFrameParity | SCRATCHPAD (0x1F800000 + 309). Double-buffer / frame parity. 0 selects the low pool half, non-zero the high half. Same byte hud_gauge_emitter.cpp reads as hudViewportY(c) = byte << 8. |
| `0x800D3E68` | u32 (address constant) | kPoolEndLow | MAIN.EXE-window address: END of the low packet-pool half, used when parity == 0. Limit = (0x800D3E68 & 0x00FFFFFF) - 4632 = 0x000D2C50. |
| `0x800E7E68` | u32 (address constant) | kPoolEndHigh | MAIN.EXE-window address: END of the high packet-pool half (matches the pool span [0x800BFE68, 0x800E7E68) already documented in overlay_gt3gt4.cpp), used when parity != 0. Limit = 0x000E6C50. |
| `0x800ED8C8` | u32 -> OT base | kOtBasePtr | MAIN.EXE global (0x800F0000 - 10040). Dereferenced to the live ordering-table base. Already named in submit.cpp / perobj_dispatch.cpp / tile_grid_layer.cpp. This function uses base+16, i.e. OT INDEX 4 — near-frontmost (the same table whose index 0x7FF, byte 0x1FFC, is the background layer). |
| `0x80108FDC` | 16 x u32 | kTrailColourRamp | OVERLAY A03 DATA, not MAIN.EXE. The A03/MODE overlay base is 0x80108F9C, so this is base + 0x40 — the data block ahead of the overlay's first function at 0x80109024. 64 bytes copied to the stack at entry; only table[0..10] are read, one per trail point. Values are content, not code — read them at runtime with mem_r32(0x80108FDC + 4*j). Guard the read with the same overlay-residency check the dispatch uses. |
| `0x80108FDC + 64 = 0x8010901C` | address constant | kTrailColourRampEnd | memcpy end bound only (this is what clobbers a0). |

### Callees

| addr | role | native equivalent |
|---|---|---|
| `0x80085690` | ratan2(dy, dx) -> 12-bit angle of the segment direction. a0 = cur.y - prev.y, a1 = cur.x - prev.x (MIPS order is y first). | Trig::ratan2 (game/math/trig.cpp:23), instance method reached as trigOf(c).ratan2(y, x). Already an installed, MIRROR_VERIFY'd override. |
| `0x80083F50` | rcos(angle + 1024) -> the perpendicular's X component before rounding. | Trig::rcos (game/math/trig.cpp:69) via trigOf(c).rcos(a). |
| `0x80083E80` | rsin(angle + 1024) -> the perpendicular's Y component before rounding. Called AFTER rcos; its argument is the untouched `a`. | Trig::rsin (game/math/trig.cpp:4) via trigOf(c).rsin(a). |
| `0x80083DE0` | libgpu draw-mode packet-header builder; called once after the loop with (P, 0, 1, 53, 0) to lay down the DR_TPAGE word 0xE1000235 (tpage 0x35, dither, semi-transparency mode 1 = ADDITIVE). | A native DRAFT exists — static guest 0x80083DE0 in game/render/wide_re_libgpu_leaves.cpp:280 — but it is deliberately UNWIRED (the file's own banner lists it as out of scope). NOT NEEDED by a read-only producer: its whole job is to fill a guest packet the producer does not create. It only matters as the SOURCE of the blend-mode constant, and that is derivable straight from the gen. WARNING: the existing draft is WRONG here — guest 0x80083DE0 computes the low mode bits as `a3 & 0x9FF` (authenticated executable/overlay evidence, `c->r[2] = c->r[7] & 2559u`) while the draft uses `a1 & 2559` and its comment asserts a3 is never read. With this call site (a1 = 0, a3 = 53) the draft would produce 0xE1000200 instead of 0xE1000235 — losing both the tpage and the additive blend. Do not wire that draft without fixing it. |

### Emit contract

- **writer:** NONE of the sprite family's writers. This function is its own writer: it inlines four 36-byte POLY_G4 (GP0 command byte 0x3A = shaded four-point polygon, semi-transparent, untextured) packets per segment plus one 12-byte DR_TPAGE, directly into the shared packet pool at *(0x800BF544). Native equivalent: four RenderQueue::emitOrQueue calls per segment with nv=4, semi=1, raw=0, mode=3 (untextured), uv all 0, tp_x=tp_y=clut_x=clut_y=0, tp_blend=1 — exactly the call shape game/render/fx_line.cpp::strokeSegment uses, but with per-vertex gouraud colours instead of a flat grey.
- **gateBias:** N/A — there is no OT-key gate and no SpriteAnchor::otKeyInRange. The only rejects are (a) the pool-room guard `(unsigned)*(0x800BF544) < ((poolEnd & 0x00FFFFFF) - 4632)`, which aborts the WHOLE function, and (b) the per-segment degeneracy history s7 (null point (0,0) or cur == prev, which also kills the FOLLOWING segment). The OT bucket is a fixed constant: *(0x800ED8C8) + 16, i.e. OT index 4 — near-frontmost, one of the very first buckets drawn last.
- **dqa:** N/A — the GTE is never touched. No FUN_800329E0, no camera load, no DQA/DQB, no RTPS, no MAC0. The geometry is already in screen space.
- **scaleFormula:** N/A in the sprite sense; the size is a fixed screen-space half-width derived from the segment angle only. Exact order (getting this backwards is a silent pixel bug): ang = ratan2(dy, dx); a = ang + 1024; perpX = (rcos(a) * 2 + 2048) >> 12 (ARITHMETIC shift, so it floors toward -inf); perpY = (rsin(a) * 2 + 2048) >> 12. The <<2 for the halo happens AFTER that rounding: perpX4 = perpX << 2. Results are integers in {-2,-1,0,1,2} (core) and {-8,-4,0,4,8} (halo).
- **ir0Formula:** N/A — no depth cue, no IR0, no scratchpad 0x1F800090, no DPCS/DPCT. Brightness modulation is the half() function on the halo quads only: half(colourWord) = (colourWord >> 1) & 0xFF7F7F7F, i.e. a 32-bit logical shift then a per-byte 0x7F mask = each of R,G,B halved independently with truncation.
- **farColour:** N/A — GTE CR21-23 are never read or written. The 'fade' is authored: colour 0x000000 (black) is hardcoded at both outer vertices (c0 and c1) of every quad, and the ramp colour sits on the two centre-line vertices (c2, c3), so each quad is a gouraud gradient from the ramp colour on the spine to black at the edge.
- **recordListFormula:** There is no record list. Geometry comes from the node's own 11-point screen array at node+0x3C (stride 4, s16 x then s16 y). Colours come from the overlay table at 0x80108FDC: segment i (i = 1..10) uses colA = mem_r32(0x80108FDC + 4*(i-1)) on its PREV end and colB = mem_r32(0x80108FDC + 4*i) on its CUR end. The colour cursor advances at the END of every iteration, INCLUDING skipped ones — so a suppressed segment still consumes a ramp entry and the ramp stays locked to the point index, not to the emitted-segment index.
- **perFrameCount:** Per node, per frame: up to 10 segments x 4 POLY_G4 quads = 40 quads, plus 1 DR_TPAGE packet. Guest pool consumption = 144 bytes per emitted segment + 12 bytes, max 1452 bytes. Zero quads if every point is (0,0) (the mode packet is still emitted whenever the pool guard passes).

**Guest writes:** "Four kinds of guest write, ALL of them packet/OT bookkeeping — a read-only pc_render producer can skip every one of them, because the guest-visible behavior still executes underneath and keeps guest state byte-exact. (1) STACK: sp descends 152, s0..s7/fp/ra spill at sp+112..148, plus the 64-byte colour-table copy at sp+24..87 and the four locals at sp+88/92/96/100 (prevPerpY, i, ptsPtr, packet1 cursor) and the two call-clobber saves at sp+104/108. Irrelevant to a producer; would only matter if this were wired as an override, in which case the frame must be mirrored per the guest-stack-residency rule. (2) PACKET POOL: 144 bytes per emitted segment at the bump cursor (the four POLY_G4 packets) plus 12 bytes for the trailing DR_TPAGE — up to 1452 bytes. Includes four DEAD pre-loop halfword stores of pts[0] into P+24/26 and P+60/62 that the first emitted segment always overwrites. (3) OT BUCKET: *(*(0x800ED8C8) + 16) is rewritten five times per emitted segment plus once for the mode packet (each prim prepends itself to the head). (4) POOL CURSOR: *(0x800BF544) is advanced once, at the very end. Note the clean early-out: if the pool-room guard fails the function returns having written NOTHING outside its own stack frame — not even the cursor. So a native producer's contract is simply 'read node+0x3C[0..10], read 0x80108FDC[0..10], draw 4 quads per live segment' with zero mem_w* calls."


### Proposed wrappers

- `struct TrailNode` lens over the node — `TrailNode(Core*, uint32_t node)` with `ScreenPt point(int i) const { return { mem_r16s(mNode + kTrailPoints + 4*i), mem_r16s(mNode + kTrailPoints + 4*i + 2) }; }` and `bool isNull(int i)`. Kills every `mem_r16(node + 0x3C + i*4)`.
- `struct ScreenPt { int x, y; };` and `struct ScreenVec { int x, y; };` so `prev - perpJoint` reads as vector arithmetic, plus `inline int s16w(int v) { return (int16_t)(uint16_t)v; }` (the same helper fx_line.cpp already declares) for the 16-bit wrap every vertex store performs.
- Named constants in an anonymous namespace, one per literal: `kTrailPoints = 0x3Cu`, `kTrailPointCount = 11`, `kTrailSegments = 10`, `kTrailColourRamp = 0x80108FDCu`, `kTrailRampWords = 16`, `kQuadsPerSegment = 4`, `kHaloShift = 2`, `kPerpQuarterTurn = 1024`, `kPerpGain = 2`, `kPerpRound = 2048`, `kPerpShift = 12`, `kHalfBrightMask = 0xFF7F7F7Fu`, `kOtBucketFx = 16u /* OT index 4 */`, `kTrailBlendAdditive = 1 /* GP0 ABR 1: B + F */`.
- `enum class SegSkip : uint32_t { None = 0, NullOrDuplicate = 1, PrevWasBad = 2 };` — a tiny 2-bit history type wrapping s7 with a `shiftIn(bool degenerate)` method, so the non-obvious 'a bad point kills TWO segments' rule is stated once in a named place instead of hiding in `s7 = ((s7 << 1) & 3) | ...`.
- `static ScreenVec segmentNormal(const Trig& trig, ScreenPt a, ScreenPt b)` — owns the `ratan2 -> +1024 -> (trig*2 + 2048) >> 12` chain and its exact rcos-then-rsin ordering, with a comment saying the result is an integer in [-2, 2].
- `static uint32_t halfBright(uint32_t rgb)` — one place for `(rgb >> 1) & kHalfBrightMask`, with the comment that it is a per-byte halve, not a word shift.
- `struct TrailQuad { ScreenPt edgePrev, edgeCur; uint32_t colPrev, colCur; };` plus one `emitTrailQuad(RenderQueue&, const TrailQuad&, float ord)` that pushes {edgePrev, edgeCur, spinePrev, spineCur} with colours {black, black, colPrev, colCur} — so the four packets become four one-line constructions of the same named struct rather than 100 lines of `mem_w16(s1 - 100, ...)`.
- `Render::fxScreenTrailRender(uint32_t node)` in a new `game/render/fx_trail.cpp` (subsystem folder rule), with the dispatch added to render_walk.cpp's type-0x20 chain as `else if (rfn == 0x801113B4u && c->mem_r32(0x801113B4u) == 0x27BDFFE8u)` — the same overlay-residency guard the other overlay producers use. Add a `PSXPORT_DEBUG=fxtrail` line (node, drawn/total segments, screen extent) mirroring fxRingSpriteRender's log, since a trail whose history is all-(0,0) and a producer that never dispatched look identical in the picture.

### Risks

- THE POINT ARRAY'S PRODUCER IS UNIDENTIFIED. I searched ov_a03 for a writer of node+0x3C..+0x67 (mem_w16 at +60/+62, and `reg + 60` address forms) and found none that matches a stride-4 11-slot fill. So I cannot say from instruction-level analysis alone whether these are (a) per-frame RTPS'd screen positions of a moving object (a projected motion trail) or (b) authored/animated screen coordinates. The read pattern is identical either way and the port is unaffected, but the ANSWER decides the fps60 story below. Do not invent a story here — confirm it live with a watchpoint on node+0x3C before claiming one.
- FPS60 / LERP CONSEQUENCE. Because the producer consumes SCREEN coordinates and owns no world state, it cannot interpolate the way every other producer in fx_sprite.cpp/fx_line.cpp does (project world state with the lerped camera). If the points turn out to be guest-projected, this producer will step at 30 Hz under the non-lerped guest camera — the exact defect kanban #23 describes for the old flame tap. Interpolating it would mean lerping the 11 SCREEN points through EffectLerp (host-side, read-only, legal) rather than re-projecting. Flag this explicitly rather than shipping it as 'done'.
- OT BUCKET 4 -> RQ LAYER IS A JUDGEMENT CALL. The guest links into *(0x800ED8C8)+16, i.e. OT index 4 of a 0x800-entry table where 0x7FF is the background — so this draws in front of essentially all 3D geometry. RQ_OVERLAY with RQ_OM_2D_FG is the closest match, but that removes it from real-depth occlusion entirely; RQ_WORLD with a near-clamped ord would keep depth interaction. Pick one deliberately and say which; do not let it default.
- PERPENDICULAR QUANTISATION. The guest rounds the half-width to an integer in {-2..2} px (and {-8..8} for the halo), so a diagonal streak steps in visible 1-px jumps and its width varies with angle (2 px on the axes, ~1.4 px rounded to 1 on the diagonals). Keeping the value in float would look smoother but is a DIFFERENT effect. My recommendation is to reproduce the quantisation (it is the authored look) and note the choice in the file banner — but this is exactly the kind of change that gets called a bug later either way.
- THE EXISTING guest 0x80083DE0 DRAFT IS WRONG AND CONTRADICTS ITS OWN COMMENT. game/render/wide_re_libgpu_leaves.cpp:280 computes `a1 & 2559` where guest 0x80083DE0 (authenticated executable/overlay evidence) computes `a3 & 2559`, and its banner asserts 'a3(r7)=UNUSED by this leaf (register alias only, verified: the guest-visible behavior never reads r7)' — falsified. It is currently `static` and unwired so nothing is broken today, but the note is a confidently-wrong RE note that will mislead the next session. Worth correcting in the same pass (per the 'keep notes honest and self-correcting' rule), though it is outside this port's scope.
- THE WRAPPER'S DEAD ARGUMENTS. a0 = 4 and a2 = 16 are provably unread in ov_a03's 0x80110B00 (r4 is overwritten by the memcpy bound in the prologue, r6 by `r6 = r5 << 2` before any read). I believe they are vestigial parameters of a shared source signature (4 quads/segment, 16 ramp words) that this build const-folded, but that is INFERENCE, not evidence — there is exactly one caller in the overlay and no sibling at 0x80110B00 in any other overlay to compare against (ov_a0b has no 0x80110B00; its 0x801113B4 is an unrelated state machine). Treat the 11/10/4/16 shape as hardcoded, which is what the instruction stream says.
- THE MODE PACKET HAS A SIDE EFFECT ON OTHER PRIMS. The trailing DR_TPAGE is prepended to bucket 4, so under psx_render it arms additive blending and tpage 0x35 for everything ALREADY queued in that bucket, not just these quads. A native producer that only draws its own 40 quads will not reproduce that. This is almost certainly harmless (bucket 4 is a near-front effect bucket) but it is a real behavioural difference between the guest path and the producer, and it is the sort of thing that shows up as 'this other effect changed colour'.
- The 16-word ramp lives in OVERLAY data at 0x80108FDC. Its contents are not in the repo (authenticated executable/overlay evidence holds code only) and I did not run the game, so I have not seen the actual colours. The port reads them at runtime, which is correct — but if the dispatch's residency guard is ever wrong, this read returns whatever overlay now occupies the window and the trail draws in garbage colours rather than not drawing at all.


---

# Wrapper / lens design

# Design: the shared wrapper layer for the sprite-effect render family

Read: `game/render/fx_sprite.{cpp,h}`, `fx_vortex.cpp`, `fx_ring.cpp`, `fx_mesh.h`, `fx_line.cpp`, `mesh_quads.cpp`, `swing_fx.cpp`, `render.h`, `render_walk.cpp`, `projection.h`, `render_internal.h`, `node_xform.h`, `trig.h`, `proj_params.h`, `render_queue.h`. Nothing edited.

---

## 0. The honest verdict first: four of the six incoming producers are NOT in this family

Before designing anything shared, the scope has to be cut, because the biggest risk in this task is a wrapper layer that grows to "cover" the six new specs and thereby smuggles a wrong gate or a wrong anchor packing into a member that never belonged.

| target | writer | DQA / MAC0 | OT-key gate | family member? |
|---|---|---|---|---|
| `0x8010C1D8` **sprite tail** | `FUN_8002847C` (36-byte) | 6 | `FUN_800317CC(-100)` clamped | **YES — full member** |
| `0x8010C1D8` **mesh pass** | inline POLY_GT4, `FUN_80027768` format | none | AVSZ4 −100, **no min-4 clamp** | no — `meshQuadRecordsEmit` scope |
| `0x8013B118` **branch A** | `FUN_8002847C` | 6 | bias 0, **clamps instead of rejecting** | **partial member** (anchors from overlay tables, not the node) |
| `0x8013B118` **branches B/C/tail** | `FUN_80027768` | none | AVSZ4 −160/0/−100 | no — `FxMesh::draw` scope |
| `0x80116904` | inline LINE_G2 + DR_MODE | **never programmed** | raw, **no pre-clamp** | **NO** |
| `0x80110CA4` | inline POLY_GT4 | **never programmed** | own 3-stage gate + pre-snap + post bias | **NO** |
| `0x801110BC` | inline 4-word rect | **never programmed** | **no OT key at all**, SX<320 clip | **NO** |
| `0x801113B4` | inline POLY_G4 | **GTE never touched** | none | **NO** |

So the six specs contain **one** full family member, **one** half-member, and **four** producers whose only commonality with `fx_sprite.cpp` is that `render_walk` dispatches them off `node+0x18`. The specs already propose separate files for those four (`fx_dotfield.cpp`, `fx_trail.cpp`, `fx_backdrop_plane.cpp`, and the A08 rain into `fx_line.cpp`'s neighbourhood) — **that is right and must not be softened.** None of the four may call `SpriteAnchor::baseScale` or `SpriteAnchor::otKeyInRange`; three of them do not program DQA at all, so `baseScale` would be a fabricated number, and all three of their gates differ from `otKeyInRange` in ways that change *which anchors draw*.

The wrapper layer below is therefore **two tiers**:
- **Tier A — genuinely universal to every type-0x20 producer** (node header lens, OT log-map primitive, depth-cue, screen-extent diagnostic). Lives in a new `game/render/fx_node.h` + existing `fx_sprite.h`.
- **Tier B — the sprite-anchor contract**, used only by the ~12 real family sites. Lives in `fx_sprite.h`.

---

## 1. The typed lens over the effect node

### 1.1 The trap, stated precisely

The family authors world anchors in **two incompatible packings that overlap in memory**:

```
packed form (node+0x2C):   X @ +0x2C   Y @ +0x2E   Z @ +0x30      three s16, stride 2
wide   form (node+0x2E):   X @ +0x2E   Y @ +0x32   Z @ +0x36      three s16, stride 4
```

`+0x2E` is **Y** in the packed form and **X** in the wide form. Both are live in this file today (`kAnchorXY` vs `kAltAnchorX`), and the incoming `0x8010C1D8` node carries **both at once** (wide mesh position at `+0x2E/32/36`, packed sprite anchor at `+0x60/62/64`). A lens with a single `anchor()` accessor returns a plausible, wrong number for half the family and nothing crashes.

The same holds for every slot past the header — the meanings are **per controller**, not per family:

| slot | meanings observed |
|---|---|
| `+0x32` | OT bias (most) · base **radius** (`fxRingSpriteRender`) · wide-anchor **Y** · unused |
| `+0x34` | 8-byte record list · packed 8.8 scale pair (`fxAnimSprite`) · 21-item ring table |
| `+0x50` | particle array base (`FN_PARTICLE`) · wind magnitude (`fxParticleField`) · **LCG seed** (`0x80116904`, `0x801110BC`) · X scale numerator (vortex) · anim record table (`kAltTable`) |
| `+0x60` | alt scale pair · jet **mode** selector · **packed anchor** (`fxRotSpriteTail`, `0x8010C1D8`) · 8-point chain array (`ropeChain`) |

**Conclusion: there must be no single `EffectNode` lens with family-level field accessors.** That design is what the brief correctly calls "worse than raw offsets".

### 1.2 The design — a header lens + per-controller lenses + named packings

**New file `game/render/fx_node.h`.** Owns only what `render_walk` itself guarantees for *every* type-0x20 node.

```cpp
// game/render/fx_node.h — the TYPE-0x20 RENDER-NODE HEADER, and nothing else.
//
// A type-0x20 node's first 0x28 bytes are WALK-OWNED: the walk reads them to decide whether and how
// to dispatch, so their meaning is fixed for every node in the game. Everything from +0x2C onward is
// CONTROLLER-OWNED and its meaning is a property of the render fn at +0x18, not of the family — the
// same byte is an OT bias in one controller, a base radius in the next, and an anchor Y in a third
// (see the table in this header's companion, fx_sprite.h). So this lens deliberately STOPS at +0x24,
// and a controller that needs more declares its OWN lens deriving from this one.
#pragma once
#include <cstdint>
#include "core.h"

class FxNode {
public:
  static constexpr uint8_t kTypeCustomRenderFn = 0x20u;

  FxNode(Core* c, uint32_t at) : mCore(c), mAt(at) {}
  uint32_t addr()     const { return mAt; }
  Core*    core()     const { return mCore; }

  bool     visible()  const { return mCore->mem_r8(mAt + 0x01u) != 0; }  // per-frame marker; the walk gates on it
  uint8_t  variant()  const { return mCore->mem_r8(mAt + 0x03u); }       // selector byte; VALUE meaning is per controller ('!' / 8 / 0x91)
  uint8_t  state()    const { return mCore->mem_r8(mAt + 0x04u); }       // behaviour state
  uint8_t  subState() const { return mCore->mem_r8(mAt + 0x05u); }       // secondary behaviour state
  uint8_t  type()     const { return mCore->mem_r8(mAt + 0x0Bu); }
  uint32_t renderFn() const { return mCore->mem_r32(mAt + 0x18u); }
  uint32_t next()     const { return mCore->mem_r32(mAt + 0x24u); }

protected:  // the ONLY way a controller lens reaches its own fields — named, in the derived class
  int32_t  s16(uint32_t off) const { return mCore->mem_r16s(mAt + off); }
  uint32_t u16(uint32_t off) const { return mCore->mem_r16 (mAt + off); }
  int32_t  s8 (uint32_t off) const { return mCore->mem_r8s (mAt + off); }
  uint32_t u8 (uint32_t off) const { return mCore->mem_r8  (mAt + off); }
  uint32_t u32(uint32_t off) const { return mCore->mem_r32 (mAt + off); }
  Core* mCore; uint32_t mAt;
};
```

`mem_r16s` / `mem_r8s` already exist on `Core` (`external/psxport/runtime/psx/core.h:99-101`) — no new sign-extension helper is needed, and no producer should write `(int16_t)c->mem_r16(...)` again.

**The two anchor packings, as two differently-named readers** (in `fx_sprite.h`, since only the sprite family has them):

```cpp
struct WorldAnchor { int x, y, z; };

// THE TWO PACKINGS OVERLAP: +0x2E is Y in the packed form and X in the wide form. There is no
// anchor() accessor and there must never be one — the packing is a fact about the CONTROLLER, so it
// is named at the call site where a reviewer can check it against the RE.
inline WorldAnchor anchorPacked(Core* c, uint32_t at) {   // X@+0, Y@+2, Z@+4  (the node+0x2C form)
  return { c->mem_r16s(at), c->mem_r16s(at + 2u), c->mem_r16s(at + 4u) };
}
inline WorldAnchor anchorWide(Core* c, uint32_t at) {     // X@+0, Y@+4, Z@+8  (the node+0x2E form)
  return { c->mem_r16s(at), c->mem_r16s(at + 4u), c->mem_r16s(at + 8u) };
}
```

**Per-controller lenses.** Each producer declares its own in its own anonymous namespace, deriving from `FxNode`, with accessors named for *what that controller means*. Example for the two incoming that earn one:

```cpp
// 0x8010C1D8 — ONE node carrying TWO anchors in TWO packings. This is exactly the node that proves
// a family-level anchor() accessor cannot exist.
class RotMeshNode : public FxNode {
public:
  using FxNode::FxNode;
  WorldAnchor meshPos()      const { return anchorWide  (mCore, mAt + 0x2Eu); }  // stride 4
  WorldAnchor spriteAnchor() const { return anchorPacked(mCore, mAt + 0x60u); }  // packed VX|VY, VZ at +0x64
  int  frameIdx()   const { return s8(0x41u); }      // the SIGNED HIGH BYTE of the u16 at +0x40
  int  euler(int i) const { return s16(0x54u + 2u * (uint32_t)i); }
  int  colScale(int i) const { return s16(0x68u + 2u * (uint32_t)i); }   // 12.12, per matrix COLUMN
  int  spriteScaleNumer() const { return s16(0x70u); }
  bool useSpriteTableA()  const { return variant() == 8u; }
};

// 0x80116904 — the falling-mote streaks. NOT a sprite-family node: no anchor packing question,
// because all three axes are separate s16 at stride 2 and there is no second anchor.
class RainMoteNode : public FxNode {
public:
  using FxNode::FxNode;
  WorldAnchor anchor()   const { return anchorPacked(mCore, mAt + 0x2Cu); }
  WorldAnchor prevBase() const { return anchorPacked(mCore, mAt + 0x48u); }  // written by the render fn's epilogue
  uint32_t    lcgSeed()  const { return u32(0x50u); }   // read once, never written back
};
```

**Rule for when a controller gets a lens rather than a `constexpr` block** (write it into `fx_node.h`'s banner, so the next porter is not guessing):

> A controller gets a lens class when **any** of: it reads more than four node fields; it reads the same field at more than one site; or it carries **two** blocks of the same kind (two anchors, two scale pairs). Otherwise the existing anonymous-namespace `constexpr uint32_t kFoo = 0x..;` block is the right weight — `waterJetSpriteRender` (two fields) must not grow a class.

Of the current producers this makes lenses for: `fxSpriteEmit`'s particle/ring variants, `fxRingSpriteRender`, `a0fVortexRender`, and the incoming `RotMeshNode` / `A04Node` / `RainMoteNode` / `BackdropPlaneNode` / `TrailNode`. It leaves `waterJetSpriteRender`, `fxAltAnimSpriteRender`, `fxRotSpriteTailRender` on constants.

**The overlapping-slot table above belongs in `fx_sprite.h`'s banner as a comment**, verbatim. It is the single most useful artifact of this exercise: it is the thing that tells the next porter that `node+0x50` is not "the LCG seed".

---

## 2. Should `project → gate → scale` become one helper?

**Yes for `project → gate → depth`. No for `scale`.** The scale is where every member differs, and it is the one line a reader of a producer most needs to see.

Measured variance across the 12 existing call sites plus the two incoming family members:

| axis | values in use |
|---|---|
| anchor source | packed s16 triple · wide s16 triple · particle array element · computed ring point · overlay anchor table · lerped via `mEffectLerp` |
| projecting xform | pure scene camera (`projComposeCamera`) · **node's own object xform** (`FN_RINGROT`, `projComposeObjectHost`) |
| DQA | 6 · 4 (`FN_PARTICLE` `'!'`, `waterJet`) · not programmed |
| gate bias | node field · instruction-stream constant (`-50`, `-100`, `0`, `-64`) |
| gate variant | clamped-to-4 · raw · clamp-instead-of-reject |
| scale | `MAC0` · `(MAC0·n)>>4` · `>>7` · `>>8` · `>>11` · `(MAC0>>8)·n` **shift-first** · `MAC0<<1` · `MAC0·3` **no shift** · `MAC0` exact · per-axis X≠Y |
| depth bias | same as gate bias · *different* from it (`fxRotSpriteTail`: gate 0, depth −100) · none |
| IR0 | 0 · `node[7]<<5` · `vecLen>>3` · `1024+rand&1023` · `4096−n<<4 + rand&2047` · inherited/unprogrammed |

Two of those axes (gate variant, shift-first vs multiply-first) are *silent* — the wrong choice produces a plausible picture with the wrong items drawn or the wrong rounding. They must be spelled at the call site.

### 2.1 The helper — exact signature

Add to `fx_sprite.h`, as **static methods on `SpriteAnchor`** (pure functions of the transform + ints; per `docs/oop.md` "pure math/utility is static"):

```cpp
#include "projection.h"   // EObjXform, ProjVtx

// WHICH OT-key gate the emitter runs. The family does NOT agree on this and the difference is
// invisible in the picture until you compare which items drew — so it is a REQUIRED field with no
// default, and every value names the guest leaf or address it came from.
enum class OtGate {
  Clamped,     // FUN_800317CC / FUN_80032EB8: k = max(4, (SZ3>>2)+bias), log-map, reject outside [4,0x7FF].
               //   The sprite family's own gate. 11 of the 12 current sites.
  Raw,         // NO pre-clamp: k = (SZ3>>2)+bias straight into the log map, so SZ3 in [0,15] is
               //   REJECTED where Clamped would accept it. 0x80116904's inline gate, and the
               //   0x8010C1D8 MESH pass. Never used with a record writer.
  ClampOnly,   // 0x8013B118 branch A: SZ3<=0 and the GTE FLAG are the ONLY rejects; the key is
               //   clamped into range rather than dropped, so every anchor in front of the camera draws.
};

struct SpriteGate {
  int    dqa;                            // the DQA the emitter programs — REQUIRED (6 / 4)
  int    otBias    = 0;                  // what the gate's key is biased by
  int    depthBias = 0;                  // what the DRAW depth is biased by. A SEPARATE number:
                                         //   FUN_8012D9E8 gates with 0 and only then subtracts 100.
  OtGate gate      = OtGate::Clamped;    // ... the one defaulted field, because 11/12 sites are Clamped
};

// One projected, gated sprite anchor: everything the family's shared opening establishes
// (FUN_800329E0's camera+DQA load, FUN_800317CC's RTPS + key gate + MAC0 publish, and the authored
// near bias), and NOTHING it does not. The SCALE is deliberately absent — see the variance table in
// fx_sprite.cpp's banner; folding it in would hide multiply-first vs shift-first, which is a real
// rounding difference this file already documents on fxCuedSpriteRender.
struct SpriteHit {
  bool    emit = false;   // false = the emitter's own gate rejected this anchor
  ProjVtx pv{};           // native projection: float screen XY + view depth
  int32_t mac0 = 0;       // the depth-cue base scale at this depth (= SpriteAnchor::baseScale)
  float   ord  = 0.0f;    // proj_pz_to_ord(pz + 4*depthBias), near-clamped: the draw depth
  explicit operator bool() const { return emit; }
};

class SpriteAnchor {
public:
  // xf is passed IN, not composed here: FN_RINGROT and 0x8010C1D8's mesh pass project through the
  // node's OWN object xform (projComposeObjectHost), not the pure scene camera, and read H off it.
  static SpriteHit project(const EObjXform& xf, const WorldAnchor& a, const SpriteGate& g);

  static int32_t baseScale(uint32_t H, int sz, int dqa);      // unchanged
  static bool    otKeyInRange(int sz, int bias);              // unchanged — == OtGate::Clamped
  static int32_t otBucketOf(int32_t key);                     // NEW: the pure log map + [4,0x7FF] test,
                                                              //   returns the bucket or -1. See §4.2.
  static uint8_t depthCue(uint8_t comp, int32_t ir0, int32_t farComp);   // see §4.5
};
```

### 2.2 What a producer then reads like

`fxRotSpriteTailRender` today (13 lines of `AltSprite` field-setting plus a 30-line `altSpriteEmit`) becomes:

```cpp
void Render::fxRotSpriteTailRender(uint32_t node) {
  const RotSpriteTailNode nd(mCore, node);
  const int idx = nd.frameIdx();
  if (idx < 0 || idx >= kRotTailTableN) return;
  const uint32_t rec0 = mCore->mem_r32((nd.useAltTable() ? kRotTailTableA : kRotTailTableB) + idx * 4u);
  if (!rec0) return;

  EObjXform cam; projComposeCamera(&cam);
  // The gate and the depth bias are DIFFERENT numbers here: FUN_8012D9E8 gates the key with 0 and
  // only then subtracts 100 from it. That is two decisions, so it is two fields.
  const SpriteHit hit = SpriteAnchor::project(cam, nd.spriteAnchor(),
                          { .dqa = 6, .otBias = 0, .depthBias = kRotTailDepthBias });
  if (!hit) return;

  const int32_t s = (int32_t)(((int64_t)hit.mac0 * nd.scaleNumer()) >> kRotTailShift);  // multiply, THEN shift
  ObjScope objScope(mCore, node);
  emitAnimQuadRecords(mCore, rec0, hit.pv.px, hit.pv.py, hit.ord, s, s);
}
```

and the incoming `0x8010C1D8` sprite tail is the same eight lines with `anchorPacked(+0x60)` and `>>11`.

### 2.3 The consequence for `Render::AltSprite`

`AltSprite` is currently the family's *de facto* wrapper, and it has the exact defect this task is about: `uint32_t anchorX` with the comment "Y and Z follow at +4 and +8" **can only express the wide packing**. That is why the `0x8010C1D8` spec proposes bolting on `bool anchorPacked`. **Do not add that bool.** It is a second axis inside an already-8-field struct, and the right decomposition is to hoist the anchor *read* out entirely:

```cpp
struct AltSprite {
  uint32_t    node = 0;
  WorldAnchor anchor{};      // ALREADY READ by the caller via anchorPacked()/anchorWide(), so the
                             // packing is visible in the producer instead of hidden behind an offset
  uint32_t    rec0 = 0;
  uint32_t    numerX = 0, numerY = 0;
  SpriteGate  gate{ .dqa = 6 };
  int         shift = 8;     // scale = MAC0 * numer >> shift. MULTIPLY-FIRST ONLY — a shift-first
                             // member (fxCuedSpriteRender) does NOT get an AltSprite; see §4.1.
};
```

With that change, `altSpriteEmit` shrinks to `project → scale → emit` and the incoming tail lands with no new field at all. `shift = 0` with `numerX = 3` also expresses `0x8013B118` branch A loop 1 (`MAC0*3`, no shift) and `numerX = 1, shift = 0` expresses loop 2 (`MAC0` exact) — both without inventing an option.

---

## 3. Conversion order — the tree never breaks

Every step below compiles standalone and is behaviour-preserving except where stated. Steps 0–5 land **before** any of the six new producers.

**Step 0 — pure additions, zero call-site edits.**
`game/render/fx_node.h` (new: `FxNode`, `WorldAnchor`, `anchorPacked`/`anchorWide`, `ScreenExtent` §4.6) · `SpriteAnchor::otBucketOf` · `SpriteGate`/`SpriteHit`/`SpriteAnchor::project` · reimplement the existing `otKeyInRange` on top of `otBucketOf`. Add `fx_node.h` to `cmake/tomba2_port.cmake`? — header-only, no source-list change needed. Nothing behaves differently.

**Step 1 — `AltSprite` loses `anchorX`, gains `anchor` + `SpriteGate`.** One file, four sites (`altSpriteEmit` + its three callers), one edit. This is the step that makes the `0x8010C1D8` tail land without `bool anchorPacked`.

**Step 2 — convert the `fx_sprite.cpp` producers to `SpriteAnchor::project`, least-entangled first**, one function per edit: `fxCuedSpriteRender` → `fxAltAnimSpriteRender` → `waterJetSpriteRender` → `fxRotSpriteTailRender` → `fxAnimSpriteRender` → `fxRingSpriteRender` → `fxParticleFieldRender` → **`fxSpriteEmit` last** (its `projectEmit` lambda plus the `FN_PARTICLE` and `FN_RINGROT` branches make it the one most likely to want a second look, and it is the only site projecting through an object xform).

**Step 3 — one `depthCue`.** Delete the file-local copies in `mesh_quads.cpp:43` and `swing_fx.cpp:47`, point both at `SpriteAnchor::depthCue`, and fix the stage-1 saturation (§4.5). Two files.

**Step 4 — `meshQuadRecordsEmit` gains `MeshQuadOpts` with a REQUIRED `vtxScale`.** Two call sites (`narration_swirl.cpp:73`, `fx_dust.cpp:209`) edited in the same commit to pass `256` explicitly. See §4.4 for why `gateBias` does *not* go in.

**Step 5 — `fx_vortex.cpp`** deletes its duplicated `kAnchorXY`/`kAnchorZ`/`kOtBias`/`kRecList`/`kClutPage` block (a verbatim copy of `fx_sprite.cpp:94-98`) and adopts the lens + helper. `fx_ring.cpp` does the same for its `kAnchorXY`/`kAnchorZ` pair.

**Step 6 — land the six new producers**, in files: `fx_sprite.cpp` gets **only** the `0x8010C1D8` sprite tail; new `fx_rot_mesh.cpp` (`0x8010C1D8` mesh pass), `fx_a04_panels.cpp` (`0x8013B118`, all four branches), `fx_rain.cpp` (`0x80116904`), `fx_backdrop_plane.cpp` (`0x80110CA4`), `fx_dotfield.cpp` (`0x801110BC`), `fx_trail.cpp` (`0x801113B4`).

**The gate for steps 0–5 is NOT a pixel A/B.** These are read-only overlay refactors, so SBS is `n/a` (behavior-map affect `none`), and a single-instant A/B screenshot is already known to report "0 px changed" for a working producer whose output is off-frame. The trustworthy gate is the `fxsprite` channel census: run the 22-area sweep recipe with `PSXPORT_DEBUG=fxsprite,fxanim` before and after each step and diff the log — identical `drawn=`/`quads=`/`scale=`/screen-extent lines is a real equivalence proof; identical pixel counts alone is not.

---

## 4. What NOT to abstract

### 4.1 The scale expression — the single most important non-abstraction
Ten distinct forms, and two of the distinctions are *silent*:
- **multiply-first vs shift-first.** `fxCuedSpriteRender` does `(MAC0>>8) * n`; every other member does `(MAC0*n)>>8`. `fx_sprite.cpp:598` already documents this as "same algebra, different rounding, so it is reproduced in the order the guest performs it rather than folded." An `AltSprite::shift` field cannot express shift-first, and it must not be extended to try (a `bool shiftFirst` would be a boolean that changes pixels and is invisible in review). **`fxCuedSpriteRender` and any future shift-first member do not get an `AltSprite`.**
- **per-axis X≠Y.** `FN_XYSCALE`, `fxAnimSprite`, `fxAltAnim`, the vortex core. Already two fields; keep them two fields, never one "scale".

The rule to write into the banner: *the scale line stays at the call site, spelled in the guest's own operation order.*

### 4.2 The OT gate — one primitive, three visible preparations; **do not** create three sibling predicates
Two of the specs propose `SpriteAnchor::otKeyRawInRange(key, bias)` and `SpriteAnchor::otKeyRaw(sz)` as siblings of `otKeyInRange`. **Reject that shape.** Three near-identically-named predicates differing by an invisible `if (k < 4) k = 4;` is precisely the failure mode this whole exercise exists to prevent — the next porter picks by name similarity and gets a gate that accepts/rejects a different set of anchors.

Instead: **one primitive on the already-prepared key**, and the preparation stays visible.

```cpp
// The LOG-COMPRESSED OT bucket the engine quantizes a depth into, and its [4, 0x7FF] range test.
// This is ONLY the map + test — it takes the key ALREADY PREPARED by the caller, because the
// preparation is exactly where the family's members disagree:
//   FUN_800317CC        key = max(4, (SZ3>>2) + bias)          <- the clamp
//   0x80116904 inline   key = (SZ3>>2)                          <- NO clamp: SZ3 in [0,15] is rejected
//   0x8010C1D8 mesh     key = AVSZ4 - 100                       <- no clamp, AVSZ4 not SZ3>>2
//   0x80110CA4 grid A   key = otz + phaseBias, pre-snapped, then a POST-map row bias and a re-clamp
// Returns the bucket index, or -1 if the key is out of range.
static int32_t SpriteAnchor::otBucketOf(int32_t key);
```

`otKeyInRange(sz, bias)` **stays** — it is genuinely one thing that 11 sites share and it is correct for all of them — reimplemented as `sz > 0 && otBucketOf(max(4, (sz>>2)+bias)) >= 0`. Members with a different preparation write the two lines and the difference is on screen in the diff.

This also retires `swing_fx.cpp:38`'s third copy of the same map (its `(k-4) >= 2044u` reject and `otKeyInRange`'s `(k-4) <= 0x7FBu` accept are the **same range**; only the pre-step differs) and gives `0x80110CA4`'s grid A somewhere honest to hang its pre-snap/post-bias without collapsing grid A and grid B into one "key" function — which, as its spec correctly warns, is exactly how the ordering goes silently wrong.

### 4.3 The record writers — four formats, keep four walks
`spriteRecordsEmit` (8-byte) · `emitAnimQuadRecords` (36-byte four-corner billboard) · `meshQuadRecordsEmit` (36-byte packed mesh) · `FxMesh::draw` (36-byte packed mesh + own transform + AVSZ4 gate). The last three share a *record layout* but not a *draw model*: `emitAnimQuadRecords` treats the corner bytes as 2-D offsets about a projected anchor and forces `semi=1`; `meshQuadRecordsEmit` treats them as model-space vertices with Z in byte 3 of each colour word and reads `semi` from bit 30. Do not unify the walks.

**Do** share the decode: a `struct QuadRec36 { uint32_t uv[4]; uint32_t col[4]; int8_t cx[4], cy[4]; uint16_t clut, tpage; bool semi, last; static QuadRec36 read(Core*, uint32_t rec); }` used by all three, with interpretation left to each. That removes three copies of the byte-offset arithmetic without touching a single behavioural decision.

### 4.4 `meshQuadRecordsEmit` — take `vtxScale` and `semiForce`, **refuse `gateBias`**
`vtxScale` is real and required: the two current callers author `×256`, the `0x8010C1D8` mesh pass authors `×8`. Make it a **required** field of a `MeshQuadOpts` aggregate, not a defaulted parameter, and edit both existing sites to pass `256` explicitly — a default would let the next porter inherit `×256` silently, which is the trap the spec itself flags.

`semiForce` is real (`0x3E` unconditionally vs record bit 30). Fine as an enum, not a bool: `SemiSource::RecordBit30` / `SemiSource::AlwaysSemi`.

**`gateBias` is not.** `meshQuadRecordsEmit` today has **no depth gate at all** — it draws every record. Adding one means adding the AVSZ4 computation the function does not currently do, and the `0x8010C1D8` mesh pass needs the *raw, unclamped* variant while `FxMesh::draw` uses the clamped one. That is a second gate variant on a shared walk with two existing callers whose behaviour must not change. Land it as `MeshGate { None, RawBias }` **with `None` as the value both existing callers pass explicitly**, and write into the banner: *if the gate variants reach three, split the walk instead of growing the flag matrix.*

### 4.5 `depthCue` — three copies, and they are not identical
`SpriteAnchor::depthCue` (`fx_sprite.cpp:79`), `mesh_quads.cpp:43`, `swing_fx.cpp:47`. Same intent; **`SpriteAnchor::depthCue` omits the GTE's stage-1 IR saturation** (it clamps only after the second stage; the other two clamp after `ir = ((FC<<12)-MAC)>>12`, which is where DPCS's `lm_B` actually saturates). For 8-bit components against the far colours in use (max `1020`) the intermediate stays inside ±4080, so all three agree on every input the game produces — but the faithful one is the `mesh_quads` form. Unify on **one** implementation with the stage-1 clamp, delete the other two, and record the range argument in the comment so the equivalence is auditable rather than asserted.

This is the one place in this whole design where three copies should collapse to one with no per-member parameter, because there is no per-member variance — it is a pure transcription of one GTE opcode.

### 4.6 The near-bias and the screen-extent diagnostic — abstract both
`pz = pv.pz + 4*depthBias; if (pz < proj_near_pz()) pz = nearPz;` appears verbatim three times (`fxAnimSpriteRender:481`, `altSpriteEmit:516`, `fxRingSpriteRender:713`). No variance; folds into `SpriteHit::ord`.

The min/max screen-extent accumulator is hand-rolled twice already (`fxRingSpriteRender:716-720`, `fxParticleFieldRender:816-820`) and three of the six specs ask for it again. A five-line `struct ScreenExtent { void add(const ProjVtx&); float x0,x1,y0,y1; int n; }` in `fx_node.h` pays for itself. **Keep it**, because a pixel A/B cannot distinguish "producer never fired" from "producer drew off-frame" — the extent line is what makes that falsifiable.

### 4.7 Do NOT build one shared LCG
Four LCG sites once the six land, and they are **not** the same recurrence:
- `fxParticleFieldRender`: `x = x*mult + 1`, reads `x & 0x3FFF` (**low** bits), with a wind term added *between* step and read.
- `0x80116904` and `0x801110BC`: `x = x*mult + 1`, reads `(int32_t)x >> 16` (**high** half), with different step/read orderings per axis.
- `0x8013B118`: `x = x*5 + 123` with `while ((uint16_t)x >= 18390) x -= 18390;` — a **16-bit compare with a 32-bit subtract**, which is not a modulo and legitimately reaches 82863.

Build **one** `struct GuestLcg { uint32_t s, mult; int32_t hi() const { return (int32_t)s >> 16; } void step() { s = s * mult + 1u; } }` for the two that match exactly (`0x80116904`, `0x801110BC`), so their "X reads before the first step, Y after the first, Z after the second" ordering is expressed as `hi()`/`step()` calls in the guest's order. Leave `fxParticleFieldRender`'s inline (different projection of the state, and the wind add sits inside the sequence) and give A04's its own named type carrying the non-modulo reduce with the comment the spec drafted. Do not retrofit either.

### 4.8 Do NOT fold `mEffectLerp` into the helper
Only `fxAnimSpriteRender` resolves its anchor through `mEffectLerp` today. Whether an effect's *anchor* needs actor-tier interpolation is a per-member fact (does the anchor move independently of the camera?), and folding `resolve()` into `SpriteAnchor::project` would silently change every other member's fps60 behaviour. The helper takes already-resolved world coordinates; the lerp decision stays in the producer.

*(Adjacent finding, not a wrapper decision: `fxRingSpriteRender`'s anchor is the node's own moving world position and does **not** go through `mEffectLerp`, while `fxAnimSpriteRender`'s does. That is a latent inconsistency worth a kanban card, not something to "fix" inside a refactor.)*

### 4.9 Two trig ports coexist on purpose — say so
`fx_sprite.cpp` calls **both** `MeshQuads::trig` (`FN_RINGROT`, reads the packed `cos<<16|sin` LUT at `0x800A6490`) and `trigOf(c).rsin/rcos` (`fxRingSpriteRender`, reads the sine table at `0x800A5AF0`). Different guest LUTs, different guest leaves — neither is redundant. A reader will assume one is dead code; add the one-line comment rather than "unifying" them.

---

## 5. Summary of concrete deliverables

| # | change | files |
|---|---|---|
| 1 | `FxNode` header lens, `WorldAnchor`, `anchorPacked`/`anchorWide`, `ScreenExtent`, the lens-vs-constants rule, the overlapping-slot table | **new** `game/render/fx_node.h` |
| 2 | `OtGate`, `SpriteGate`, `SpriteHit`, `SpriteAnchor::project`, `SpriteAnchor::otBucketOf`; `otKeyInRange` reimplemented on it | `fx_sprite.h`, `fx_sprite.cpp` |
| 3 | `AltSprite`: `anchorX` offset → resolved `WorldAnchor`; the four scalar gate fields → one `SpriteGate`; `shift` documented multiply-first-only | `render.h`, `fx_sprite.cpp` |
| 4 | 12 project→gate→scale sites converted (order in §3) | `fx_sprite.cpp`, `fx_vortex.cpp`, `fx_ring.cpp` |
| 5 | `QuadRec36` shared decoder | `fx_sprite.cpp`, `mesh_quads.cpp`, `fx_mesh.cpp` |
| 6 | one `depthCue` (stage-1 saturation fixed), two copies deleted | `fx_sprite.cpp`, `mesh_quads.cpp`, `swing_fx.cpp` |
| 7 | `MeshQuadOpts` with **required** `vtxScale`, `SemiSource`, `MeshGate{None,RawBias}` | `render.h`, `mesh_quads.cpp`, `narration_swirl.cpp`, `fx_dust.cpp` |
| 8 | `GuestLcg` (two users only) | `fx_node.h` |
| 9 | duplicated `kAnchorXY`/`kAnchorZ`/`kOtBias`/`kRecList`/`kClutPage` blocks deleted | `fx_vortex.cpp`, `fx_ring.cpp` |

**What this design refuses to do, restated:** no family-level `anchor()`; no `otKeyRaw`/`otKeyRawInRange` siblings; no `bool anchorPacked`; no scale folded into the helper; no unified record walk; no defaulted `vtxScale`; no shared LCG across four different recurrences; no `mEffectLerp` in the helper; and no `SpriteAnchor` method reachable from the four incoming producers that are not in this family.
