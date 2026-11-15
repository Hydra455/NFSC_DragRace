This document records the wheel-physics bindings retained from v0.5.2. For v0.6.1 target selection and temporary navigation sampling, see NATIVE_LANES.md.

# v0.5.2 native steering evidence

All VAs below refer to the supplied Carbon EXE at preferred image base 0x400000. Runtime code uses RVAs. Evidence is from the supplied EXE and IDA data.

## Confirmed angle units

| Binding | Binary operation | Unit |
|---|---|---|
| GetMaxSteering 0x6F8AA0 | Read [ISuspension+0xCC], multiply by float at 0x9C2480 | Returns turns |
| Constant 0x9C2480 | Float 0.00277777994, approximately 1/360 | Degrees to turns |
| Constant 0x9C1758 | Float 6.28318548 | Turns to radians |
| GetWheelSteer 0x6F8A70 | Read [ISuspension+0xC4+4*wheel], multiply by float at 0x9C2488 | Returns turns |
| Constant 0x9C2488 | Float 0.159154937, approximately 1/(2*pi) | Radians to turns |

The observed native maximum 0.125 is 1/8 turn = 45 degrees. v0.5.1 incorrectly labeled and used it as radians. v0.5.2 multiplies it by 2*pi before passing it to TrackLane. CheckCode also validates the native degree-to-turn constant in memory.

## Input to physical wheel path

1. Chassis state builder 0x6EE2E0 calls IInput virtual slot +0x10 at 0x6EE50D. It clamps the result into chassis-state +0x8C.
2. SuspensionRacer::DoWheelForces 0x6FA390 calls SuspensionRacer::DoSteering 0x6EF820 at 0x6FA3E2 with that state and two vector outputs.
3. DoSteering uses the human/player angle helper at 0x6EF540 when the relevant IHumanAI predicate is true. That helper reads state +0x8C, performs device/speed/sensitivity/rate processing, and returns an angle in turns. Its final operation multiplies a degree value by 0x9C2480.
4. DoSteering stores the chosen angle at full suspension +0x19C, passes it to native steering-vector generation 0x6DE970, and writes front-wheel radian angles at full suspension +0x108/+0x10C. GetWheelSteer corroborates those locations: ISuspension is full suspension +0x44, with angles at interface +0xC4/+0xC8.

v0.5.2 hooks DoSteering and the player-angle helper. The earlier generic IInput steering-getter hook is removed. Only a verified active player suspension can establish the angle-override context. Its full vtable must be 0x9E5EE8, owner at +0x2C and IVehicle at +0x40 must match the engine's verified binding, and IInput at +0x54 must equal the input pointer read through a live engine simulation callback.

The local chassis state receives the cached normalized command. A thread-local object/race context then permits the player-angle helper to return the requested turns after its native bookkeeping. The original DoSteering and remaining wheel-force path execute normally. No shared input scalar, AI target, native vtable, car transform, or velocity is rewritten. When the context, race, option, age or engine-state checks fail, original behavior is used.

## Route and player bindings retained

| Binding | VA/layout |
|---|---|
| Human update | 0x43C670, root human vtable 0x9C5260 slot +0x28 |
| Human raw steering input | IInput at full human +0x4C; scalar at full human +0x108 |
| Navigation | Full human +0x7C; nav vtable 0x9F30D8, valid byte +0x58 |
| Navigation point / tangent | Nav +0x94 / +0xB8 |
| Optional boundary curves | Nav +0xA0/+0xAC when +0x5D is enabled |
| Position | ISimable slot +0x60, 0x6D7EA0 |
| Travel direction | IVehicle slot +0xCC, 0x6C6610; Vector3 output argument |
| Suspension | Full human +0x100; interface vtable 0x9E5E68 |

The geometric controller, three offsets and prototype wheelbase remain new integration code. They do not constitute a recovered Carbon drag-lane routine or a port of MW's physical lane metadata. Geometry and wheel-angle logs are automatic because running hooks alone did not prove successful driving in the previous versions.
