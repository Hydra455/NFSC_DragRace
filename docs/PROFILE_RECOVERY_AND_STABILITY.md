# v0.7.1 implementation details

## Native evidence and limits

Carbon's profile lane count helper 0x7F8270 counts low-nibble type 1. Generic native boundary processing at 0x7FA710 and related loops instead tests the type bit against a native mask table at 0x9F2E18. Entry 5 at 0x9F2E2C contains 0x402, allowing types 1 and 10. The entry's existence and use as a native eligibility mask are verified; the claim that the v0.7 failed section contains type 10 is still a hypothesis because that log did not include raw profile types.

The resolver preserves type-1 results first, then uses only this narrow extended native mask. It tries both endpoint profiles of the same segment. Native boundary construction selects the endpoint separately while using the navigator travel direction for its orientation flags. The resolver retains that travel-direction orientation for both endpoint decodes and sorts by the sampled physical axis. Both lane-offset probes and preview reapplication checks remain in place.

The existing record layout, signed 14-bit offset extraction and runtime offset scale remain unchanged. Failed header, scale, offset and no-driving cases now have distinct diagnostics. No synthetic lane grid, road-width guess, or previous-segment cache is introduced.

## Steering envelope

Given geometric normalized demand G, speed V, and native maximum steering converted to radians M:

    reserve = atan2(2.7 * 6.0, max(abs(V),10)^2) / M
    limit = clamp(1.5 * abs(G) + reserve, 0.03, 0.65)
    bounded = clamp(nativeOutput,-limit,+limit)

The 2.7 m wheelbase approximation is inherited from the geometric fallback. The reserve and multiplier are plugin tuning values, not recovered native tuning. The limit is at least the geometric steering magnitude over its existing [-0.65,0.65] range, so the fallback's nominal path demand is preserved. It bounds large additional native corrections rather than resetting or editing native controller internals.

The requested bounded output passes through the existing slew limiter; its result is then constrained to the current limit too. Invalid geometric heading or nonfinite inputs return game steering. The native controller only runs for a valid forward geometric target. Lane-offset blending uses 3.0 m/s.

## Validation

See VALIDATION.json and STEERING_REPLAY.json. Portable tests include synthetic type-10 and alternate-endpoint profiles, deterministic invalid-profile rejection, and closed-loop idealized tracking with brief saturated input spikes. Passing tests establish implementation/model behavior, not game stability or recovery on the original route. New profile-source counters and exact decode reasons are required to verify those outcomes.
