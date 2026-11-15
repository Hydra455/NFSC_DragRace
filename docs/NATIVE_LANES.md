Lane-target generation is retained from v0.6.1. v0.6.2 changes the controller receiving these targets; see NATIVE_CONTROLLER.md.

# v0.6.1 native profile targets

## Retained native calls

| Operation | Carbon VA |
|---|---|
| Segment profile lookup | 0x7EB290, retained IDB name GetSegmentProfile |
| Native lateral-offset setter / geometry reconstruction | 0x802800 |

The runtime no longer calls 0x801D00 or 0x802EA0 and no longer writes navigation mode +0x74 to 1. That temporary traffic context did not establish a correct drag target and introduced dependence on traffic-only lane policy.

The profile comes from the live segment at native table 0xB77ECC, with a 22-byte stride and direction at nav +0x88. The native lookup resolves endpoint node -> node road index -> 64-byte profile. Profile count must be 1..15; packed lane records begin at +4, and the side split is byte +1.

## Signed centres

The supplied MW WRoadNav::ChangeDragLanes at 0x786330 explicitly applies both record orientation and side polarity before calling its offset setter 0x778250. Carbon's retained native code corroborates the record stride/type/offset encoding and runtime scale, while its traffic-specific adjacent routine is not an interchangeable drag selector.

For each drivable record (low nibble equal to 1), v0.6.1 sign-extends the upper 14 bits (arithmetic-shift-by-18 equivalent), multiplies by Carbon's live scale at 0xB78C8C, negates records before the side split, and applies the segment/direction reversal. Segment flag bits 10/9 are chosen by nav direction; the reversal follows the supplied MW drag routine's XOR with direction==0. This is an integration rule verified against that disassembly, not a recovered Carbon drag symbol.

Equal magnitudes on opposite sides are therefore distinct signed targets. Coincident decoded centres are removed. Selection uses the resulting physical left-to-right order rather than raw record numbering or traffic-side boundaries. Lane types other than 1 are excluded.

## Axis and continuity

A scoped sampler calls the native offset setter for 0 and +1 while keeping the original navigation mode. It checks finite geometry, horizontal displacement 0.25..2 world metres and a predominantly lateral response. The probe's sign against the native travel-right vector orders targets physically. It then samples the selected/interpolated native offset exactly and rejects a nonzero command that produces negligible displacement.

State retains the selected relative rank, not the nearest lane to the moving car. It remaps that rank when drivable count or orientation changes. The current offset is sign-remapped when the native positive-offset axis reverses. Initial selection alone chooses the nearest centre to the car's projected position. Per-request logs separate deliberate accepted changes from raw-index changes caused by profile reordering.

## Restoring navigation

NativeNavSnapshot saves the original mode and 32 control bytes at +0x2E0. On exit it restores controls before forcing native geometry reconstruction at the original +0x2E4 offset, then restores controls again so an original pending transition survives. No whole-object copy of WRoadNav, inline splines, intrusive lists or ownership pointers is used. Native calls run outside the shared mechanics lock on the human-update callback; publication is gated by race and options generation afterward.

The tests exercise the restoration protocol with a simulated callback and the decoding/continuity logic with fixtures. They do not execute Carbon's native helpers. Physical lane motion, road joins, unequal endpoint profiles and the exact MW permission policy still require gameplay validation.

## Documentation reference

MWEncyclopedia was reviewed at commit 3c6efd35f873a82045c240c54d828e28546cf6f7: https://github.com/TsyVM/MWEncyclopedia

Its C55/C60/C47/C18 chapters orient the systems but do not provide the exact missing drag-steering implementation. Native addresses, packed-field interpretation and the side-polarity comparison above are derived from the user's supplied EXEs and IDA data.
