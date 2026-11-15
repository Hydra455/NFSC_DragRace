# Native steering integration evidence

Addresses refer to the supplied retail EXEs at preferred image base 0x400000. Runtime code uses RVAs.

MW Human Update 0x42F140, at 0x42F4A8..0x42F4BA, dispatches IVehicleAI slot +0x24 (DoSteering) and +0x3C (SetDriveTarget), passing WRoadNav +0x98 as target when IsDragSteering succeeds. The original MW human driving update uses its native steering controller. These operations are visible in the supplied MW disassembly and are more specific than the encyclopedia's general description.

Carbon human root vtable 0x9C5260 has native OnSteering 0x431700 at +0x3C. That entry tests DriveFlags bit 0, uses the DriveTarget at full human +0x98, native rigid-body velocity/direction and steering/PID objects, and writes normalized input at full human +0x108 plus debug value +0x1D8. It normalizes through the native suspension's steering-range accessor. The controller is called directly after validating its code signature and the live human object layout.

Required native controller objects are at full human +0x100, +0x1CC, +0x1D0 and +0x1F4. HumanSteeringSnapshot saves flags +0xF4, desired speed +0x94, target Vector3 +0x98, raw steering +0x108 and debug steering +0x1D8. Configure sets flags=1 (steering only), desired speed=80 and the selected target. The desired speed is temporary forward steering demand, not a throttle command; no gas/brake function or full OnDriving call is made. The output is captured before the snapshot restores all five fields. Native PID state is not copied or rolled back.

The normalized result drives the existing DoSteering/player-angle hooks established in v0.5.2. Native maximum steering remains in turns for angle actuation. The geometric fallback uses the corrected radian conversion. No original input getter is replaced.

The v0.5 native-controller experiment lacked the later post-filter actuator; it is not evidence that this integrated path succeeds or fails. The code uses an existing native controller, but this is not a recovery claim for Carbon's original cut drag controller. Actual v0.6.2 lane response, stability and vehicle-dependent tuning require gameplay validation.
