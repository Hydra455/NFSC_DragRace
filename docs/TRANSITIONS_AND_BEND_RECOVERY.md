# v0.7.2 motion and preview policy

## Target motion

For capped physics step dt=min(seconds,0.05):

    desiredVelocity = clamp(2 * (target-current), -3, +3)
    newVelocity = oldVelocity + clamp(desiredVelocity-oldVelocity, -6*dt, +6*dt)
    current += 0.5 * (oldVelocity+newVelocity) * dt

When position is within 0.0005 native units and velocity is below 0.005 units/s, the target settles exactly. The final small settle snap is separate from the acceleration limiter. Coordinate-axis reversal negates position and velocity together. Requested physical lane rank and endpoint-profile selection retain v0.7.1 behavior.

The parameters are plugin tuning, not recovered native drag constants. The current one-native-unit-to-metre assumption is checked by existing offset probes within the established 0.25–2 m/unit validity range; stored velocity itself is in native units/s.

## Preview trial transaction

Try normal, 0.55x and 1.65x preview distances, clamped to 5–42 m, only when the previous result has invalid geometric bearing or fails in native preview advancement. Stop immediately on unrelated table/axis/world/position failure. Each trial gets a fresh copy of the pre-update lane state and the same pending request. Commit the first usable target/state pair, clear its pending request once, then call the native steering controller once. If none is usable, leave the selected state/pending request unchanged and restore game steering.

The separate temporary native navigator is constructed and destroyed in each attempted sample. No live navigator geometry is edited. The existing forward-bearing check, heading limit, finite/distance/height checks and native offset readbacks are retained.

## Evidence limits

The v0.7.1 log verifies type-10 usage and some settled lane changes, but only retained successful target geometry for most failures. The new rejected-candidate geometry and retry counters are needed to verify where preview changes help. Synthetic bend/displacement regressions validate the proposed geometry cases; they are not a replay of the missing rejected game targets.
