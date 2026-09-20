# Shoulder rig inspection

Read-only editor run `CameraShoulderRigAudit-20260920-002555-9529a997` exported
the shared third-person rig, Far/Balanced/Close aim wrappers, main camera asset
and camera director. It exited normally without saving assets or maps.

`CameraRig_ThirdPerson` already exposes `Offset` as Vector3d, targeting
`OffsetCameraNode_0.TranslationOffset`. Its evaluation array applies crouch
offset, boom arm, position damping and then that final offset. Its other exposed
parameters are CrouchOffset and Forward/Lateral/VerticalDampingFactor.

Each inspected aim wrapper supplies `Offset=(-100,60,60)` cm. The Close wrapper
also has a .2-second SmootherStep entrance blend. These wrappers use private
compiled override variables; their exported IDs are implementation details and
are not a stable runtime API for writing a shoulder value.

The main camera asset uses `CameraDirector_SandboxCharacter`. That director's
export contains neither `Shoulder` nor `GetCameraState`. Its existing public
asset parameters cover FOV, crouch, driving and ragdoll offsets, but do not expose
the aim wrapper's lateral offset through a shoulder-specific interface.

Therefore the handoff's claim that no usable rig parameter exists is too broad.
There is an authored lateral-offset input to extend. Remaining implementation
must provide an owned, runtime-settable path from the already resolved shoulder
state, preserve each style's authored boom/FOV/height, and preserve collision,
crouch, cover, first-person and blend behavior. An isolated sign flip or an
unbound left-camera asset is not completed shoulder swapping.

Acceptance requires visible left/right composition changes in real play,
unchanged subject framing/aim direction, unobstructed wall/corner behavior,
appropriate transitions, stronger camera-claim precedence and correct restore.
No runtime shoulder behavior, rig edit or new binding is claimed by this audit.

The controller targeting chords are already implemented separately; their
controlled action-trigger test does not establish physical key routing or wheel
button conflicts. See `AurelionTargetingChords-2026-09-19.md` for that exact scope.
