# 0032 — keyed-producer tests compared keys with the scope serial

Status: closed.

## Observation

`tomba_native_override_catalog`, `tomba_producers`, `tomba_model_emitters` and `tomba_render_record_pool`
failed on main: every `core.emission.keyFor(packet) == RecordKey{P, obj, element, 0}` was false.

## Cause

`RecordKey::serial` (`psxport/runtime/psx/present/frame_record.h`) is a new value for every object scope
(`EmissionScope::Guard`), and `RecordKey::operator==` compared it. The tests asserted object identity
`(P, obj, element, part)` and so never matched a key carrying a serial. The shipping code was right
(`psxport/docs/presentation.md`: the serial "names a state, not an object"); the tests asserted the
pre-serial contract.

## Fix

`RecordKey::identity()` and `EmissionScope::identityFor` give the key without the serial; the tests use
`identityFor`, and `keyed_blend.cpp` uses `identity()` in place of its private copy. Same change in
Crash Bash and Toy Story 2 producer tests.
