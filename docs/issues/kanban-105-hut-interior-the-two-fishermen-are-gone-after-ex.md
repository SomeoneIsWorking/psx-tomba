---
id: kanban-105
title: Hut interior: the two fishermen are gone after exiting and re-entering the house (live)
status: open
labels: [bug]
---


USER 2026-08-19, live windowed run (pc_faithful + pc_render, 4:3, fps60=1), area 0 hut interior at live frame 55230: the two fisherman NPCs that should be in the hut are ABSENT after leaving the house and coming back. Reported right after a bout of F5 render-path switching, so the switch was the first suspect.

EVIDENCE CAPTURED FROM THE LIVE WINDOW (not re-played, not reconstructed):
  scratch/screenshots/live/hut_fishermen.png    the scene as the user sees it
  replays/bugs/hut-fishermen-missing.pad        55229 frames, the whole session, padrec save
  scratch/logs/ents_hut.txt                     both entity lists at that instant (151 entities)
Reproduce: PSXPORT_PAD_RESUME=replays/bugs/hut-fishermen-missing.pad ./scratch/bin/tomba2_port scratch/bin/tomba2/MAIN.EXE  (~9 min of fast-forward; add PSXPORT_DEBUG_SERVER=<port> to drive it)

F5 IS RULED OUT AS THE CAUSE OF STATE LOSS, measured rather than argued: two identical replay legs, one with a live renderpath psx->native round trip mid-run, both dumping 2 MB of guest RAM at pad frame 1700 -> 0 of 2097152 bytes differ. The comparison is not blind: the same pipeline with a one-byte poke (w8 800BF880 55) reports exactly 1 differing byte at that address. Scope of that negative: one replay, one scene, one round trip, 4:3 + fps60 on, NOT across a scene transition. What the switch DOES change is the presented picture's centring (the two present paths differ), which is what the user noticed alongside this.

So the likely neighbours are #47 (pc_skip: entering 'House on the Point' corrupts state — music stops, camera unfollows) and #57 (hut interior decorations missing, suspected occlusion), not the renderer toggle. NOT yet determined: whether the two NPCs are absent from GUEST STATE (a load/unload or behaviour bug) or present-but-unproduced (a render gap). The entity dump above is the start of that: neither list shows an obvious interior NPC, so find where the hut sub-scene keeps its objects before concluding.
