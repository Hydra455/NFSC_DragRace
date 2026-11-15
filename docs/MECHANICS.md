# Initial engine mechanics

This is an original integration layer using Carbon's normal-engine routines, informed by the dedicated MW drag-engine comparison. It is not a binary transplant, recovered original Carbon drag code, or an exact MW engine port.

## Implemented behavior

| Component | Behavior |
|---|---|
| Manual shift guard | Suppresses normal player AutoShift only in forward gears within the selected active race; native neutral/reverse handling remains. |
| Shift observer | Wraps the normal gear-change routine, retaining its result and recording only accepted upshifts from a forward gear. Downshifts and rejected changes are not graded. |
| Shift grade | Based on pre-change RPM divided by the car's native redline. |
| Torque bonus | Multiplies the normal engine-torque result for the scoped player engine; native torque generation is retained. |
| Over-rev model | Optional redline heat; on full heat calls Carbon's native engine-damage Blow method. Disabled by default. |
| Readout | Player binding, RPM/redline, gear, most recent grade, torque multiplier, heat, observed native blown state, shift counts, and blocked automatic-shift calls. |

Initial tuning:

- EARLY: ratio below 0.90.
- GOOD: ratio from 0.90 up to 0.96.
- PERFECT: ratio from 0.96 through 1.015.
- LATE: ratio above 1.015.
- GOOD bonus: starts at +4% torque.
- PERFECT bonus: starts at +8% torque.
- Bonus decays linearly to zero over 0.8 simulation seconds.
- Damage heat rises at 0.5 per simulation second at/above redline and cools at 0.75 per second below it.
- Each model update uses at most 0.1 seconds. Staging does not accumulate heat.

These values are prototype tuning and can be changed in `src/DragMechanics.h`. MW has additional drag-specific shift-potential, slip/traction, boost, heat, and damage behavior which this model does not reproduce exactly.

## Verified native bindings

All RVAs below are relative to the loaded Carbon module. The supplied EXE is checked by SHA-256 before enabling the experiment, and relevant code prefixes are checked in memory.

| Routine | RVA |
|---|---|
| Normal engine AutoShift | `0x002FFBD0` |
| Normal engine OnGearChange | `0x002E3590` |
| Normal engine OnTaskSimulate | `0x002FFE60` |
| Normal engine GetEngineTorque | `0x002E31E0` |
| IEngine GetRPM / GetRedline | `0x002F0FE0` / `0x002F1010` |
| IEngineDamage Blow / IsBlown | `0x002F12C0` / `0x002F1190` |
| IVehicle GetSimable | `0x002D7EC0` |
| ISimable IsPlayer | `0x002D6C30` |
| IVehicle GetDriverStyle / IsStaging | `0x002D7F70` / `0x002D7FD0` |

The normal-engine constructor at `0x00705520` establishes its interface offsets: ITransmission at +0x44, IEngine at +0x4C, and IEngineDamage at +0x70. Its base constructor at `0x006CCA00` finds IVehicle and stores it at +0x40. The vtables identify the engine and vehicle interfaces before helper calls. The ISimable vtable exposes IsPlayer at slot +0x24.

The actively-racing byte is at race-status offset `0x6A3E`, corroborated by SetActivelyRacing at `0x0064A6C0`. The existing race-parameter and collection-key gates are retained from v0.3.

## Hook and threading limits

Game callbacks perform the native computation outside the mechanics lock whenever it could re-enter a hooked function. The UI and game callbacks share model state through the existing synchronized gate. The native Blow action is called after releasing that gate. The lane tracker uses read-only native position/direction/steering-range helpers and performs no native AI steering call. No GUI or file I/O occurs in the physics callbacks.

Binding is to the first matching player engine observed in its simulation tick for one armed test. Torque-query and gear callbacks do not choose the engine. Native driver style is logged rather than used as an eligibility restriction. A changed engine object or restarted race requires a fresh disarm/arm cycle. The model does not repair the car, rewrite transmission options, alter opponent input or add a Drag menu entry. Optional lane assist and a desktop HUD are described in README.md and LANE_BINDINGS.md.

The panel is an observation aid. Its values can remain as the last observed state after a race; the active-race gate prevents engine modifiers outside the scoped race even while those last values remain visible.
