# Application composition owner

Tomba! 1 has no entry point of its own. The product's `TombaCatalog` (root `game/core/entry/`) holds
this title at index 0 and runs it through psxport's `ProductHost`, either from the picker or from
`tomba2_port scratch/bin/tomba1/SCUS_942.36`.

- `tomba1_identity.{h,cpp}`: `tomba1::identity()`, the executable's size, SHA-256, PS-X EXE header and disc
  facts (`PSXPORT_TOMBA1_DISC`, `SCUS_942.36` on the disc). Every value comes from `executable.json`
  through the compile definitions `cmake/tomba1_runtime.cmake` sets. The host extracts and authenticates
  the executable from that disc into `scratch/bin/tomba1/` when it is missing.
- `tomba1_runtime.{h,cpp}`: `Tomba1Runtime`, the `GameRuntime` the host drives: override binding, the
  finite boot prefix, the frame driver, the guest program/pad/CD-callback layouts and the code-module
  landing at `0x800E7388`.

`tomba1_runtime` exports only this directory's headers; the per-`Core` `Context` is in `../state/`, and
every other concept directory is private to the library and Tomba! 1's tests.

Nothing here parses, registers or forwards a 60fps or native-rendering option; Tomba! 1's only title
enhancement is widescreen.
