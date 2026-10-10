# 0035 — warp to area 4 or 5 aborts in the area handler

Status: open.

`warp 4` and `warp 5` from the field (after `@padgame` to the GAME stage) run the game's transition, load the
area, and then abort with `dispatchObj required a completed guest call, but execution exited as budget-exhausted`:
area 4 at `0x80117CA4`, area 5 at `0x8013500C`, each after about 564k cycles. Area 4 is a `DrawSync(0)` spin in a
handler that waits for a draw the product never completes inside one turn.

Areas 1, 2, 3 and 6..11 land (issue 0034); 0 and 12..21 were not run. Area 5 aborted before the warp moved onto the game's transition (issue 0026).
Whether a natural door into 4 or 5 aborts the same way is not yet shown, so the cause may sit in the area handlers
rather than the warp.

Next: reproduce with a natural entry, then find which guest wait the product never satisfies.
