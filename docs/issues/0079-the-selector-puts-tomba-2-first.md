# 0079 — The selector puts Tomba! 2 first

Status: fixed (2026-11-27). Owner: `game/core/entry/catalog.{h,cpp}` (`Catalog`).

## Observed
User report 2026-11-21 ("why is Tomba 2 first?"): the selector's left panel and the first name in its band were
Tomba! 2, then Tomba!.

## Cause
`Catalog` listed Tomba! 2 at index 0 on purpose, and the selector takes the catalog's order.

## Fix
The catalog lists the titles in release order: Tomba!, then Tomba! 2, as Spyro's 1, 2, 3.

## Proof
`tomba2_port` with no arguments, hidden, debug-server walk (`scratch/picker/`): Tomba! is the left panel, in colour
and first in the band. After `select right`, Tomba! 2 is in colour. `pick tomba1` boots SCUS_942.36 and
`pick tomba2` boots SCUS_944.54.
