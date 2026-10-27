# 0043 — The rope anchor producer records quads the composed present never draws

Status: open (2026-10-27). Owner: not yet attributed (`AnchorQuads::rope`, A00 `0x8013E9D8`, `docs/producers.md`;
the check is psxport `present/present_ledger.cpp`).

## Observed
Start-spam route (`scratch/player-bugs/start-spam/spam_120.pad`, unpaced, ires 0, sink 3840x2160,
`tools/shipping_settings.ini`, 1900 fence steps; `scratch/player-bugs-2/spam-timed/run.log`): 1243 error lines, 1194 of
them `[ledger:error] fN producer 0x8013e9d8 recorded 6 primitive(s) and the composed present drew none`, one per
fence from f707 to f1900, i.e. every gameplay present once the area with the rope anchor is up. The rest are
`0x80122974` (`AnchorQuads::chain`, 34) and `0x80106690` (DEMO edge sprites, 15). The same counts are in the earlier
`spam-fixed2` and `spam-cw` logs, so the defect predates the SPU and key-off work. The 40 s paced runs only reach the
attract screens, which is why they report no error lines.

## Not yet known
Whether the six quads are missing from the picture (a visible rope/anchor gap) or recorded under an object the
composer then drops. Next: a present shot of a gameplay field at f708 beside the device picture of the same field,
then trace the record from `AnchorQuads::rope` into `FrameComposer`.
