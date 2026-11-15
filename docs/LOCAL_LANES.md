# Local lane target implementation

## Native bindings (supplied Carbon EXE)

| VA | Operation | Stack arguments |
|---|---|---|
| 0x806820 | WRoadNav constructor | none |
| 0x7F7BF0 | WRoadNav destructor | none |
| 0x80F180 | InitAtPoint | position pointer, direction pointer, bool, float; callee pops 16 bytes |
| 0x80C600 | IncNavPosition | distance float, direction pointer, float, bool; callee pops 16 bytes |
| 0x7EC0D0 | SetPathType | integer enum |
| 0x6C0D40 | ISimable/PhysicsObject GetLinearVelocity | output Vector3 pointer |

Carbon human construction at 0x43B939 allocates 0x30C bytes before calling 0x806820. The destructor at 0x7F7BF0 releases owned resources without freeing the object storage. Carbon's temporary-nav sequence at 0x40B54A constructs a local navigator, sets mode 2, sets path type 4, and initializes it at the vehicle with bool false and float 1.0. This implementation follows that sequence, copying only four navigation policy bytes at +0x59..+0x5C from the live navigator. No owned pointer fields are copied.

The navigator's scope ends on every return path, invoking its destructor. These are ordinary scoped native calls, not additional hooks. The six bindings have 16-byte guards checked before Arm, alongside the existing guards.

## Sampling

Initialize at player position and blended direction. Probe offsets zero/one to obtain the local physical offset axis. Reject nonfinite/implausible axes, positions on a different vertical level, and a road centre more than 35 m away. Decode type-1 native driving lanes and preserve the selected relative physical rank. Probe the blended selected offset and verify its displacement.

Read the actual player's lateral coordinate and measure separation from both the final selected centre and the current blended centre. Advance the temporary navigator using the live navigator's forward route point as a direction hint. Preview distance is clamp(8 + 0.20*speed_mps, 10, 26). Probe the offset axis again at the preview point, reapply the selected offset with the new physical sign, and verify the target displacement before publishing it.

At junctions, the temporary navigator uses native path selection with the route direction hint; correct branch selection on every route is not established. Sampling-stage and segment diagnostics expose failures. Invalid sampling restores original game input instead of inventing a lane or forcing road centre.

## Retained steering and scope

The existing native controller at 0x431700 processes the new target. Its temporary fields are restored as before. The suspension/player-angle hooks from v0.5.2 are unchanged. There is no direct vehicle-position or velocity write and no lane teleport.

The engine/race checks, active player identities, Arm-only hooks, generation checks and result-age limit are retained. Hook bodies perform no GUI or file I/O.

## Status

The previous full-race log establishes accepted inputs and wheel commands, but the user confirms no visible lane changes. It also shows local sampling absent for at least 35 seconds and zero-offset road following. This version changes the target construction and removes that zero-offset behavior. Native execution and visible lane changes still require testing in Carbon; the sole gameplay cause is not proven here.
