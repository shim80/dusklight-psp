# Link collision and camera classification

## Result

`COLLISION_DOWNSTREAM / CAMERA_PRESERVE_CLOSED_LAYER`

For `link_camera_follow` and `link_collision_wall`, floor identity/contact is
equal through the tick-31 grounding boundary. Before that boundary, maximum
actor-collision component difference is `0.01` source unit and is confined to
the existing trace precision around the shared capsule/origin values.

The first tolerance-breaking collision event occurs at tick 32 in both
scenarios, on `origin[2]`/`bottom[2]`. It is simultaneous with actor-transform
and model-base drift and therefore downstream, not an independent collision
query defect.

The cached camera trajectories retain a bounded chase-transient residual
(maximum `0.455` source unit before tick 31). This is not new evidence: CAMERA
was already progressed and closed by the validated style-45 startup,
pre-camera-update checkpoint, prior-direction chase transient, and one-tick
process-order corrections recorded in causal iterations 6, 7 and 16. No camera
field is the current first divergence, so this pass does not reopen that layer
or modify its accepted contract.

No collision geometry, capsule, camera, actor, grounding, or tolerance was
changed. No PPSSPP run was performed.
