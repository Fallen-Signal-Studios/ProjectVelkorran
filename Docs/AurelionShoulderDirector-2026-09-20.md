# Owned protagonist shoulder director

Tarrik and Selene now reference `CA_SovProtagonist`, whose compiled
`BP_SovCameraDirector` consumes the resolved native shoulder state. Its explicit
SovPlayerCharacterBase target supports either protagonist. Left resolves to -1;
Right and Unchanged resolve to +1. The non-mount path writes the six owned
Far/Balanced/Close Aim and Strafe rig parameters before selecting the active rig.

Aim offsets remain (-100, +/-60, 60) cm. Strafe offsets remain Far
(-300, +/-80, 60), Balanced (-225, +/-60, 60), and Close (-100, +/-80, 60).
Balanced Strafe uses a single-parameter setter to preserve its CrouchOffset.
Existing FOV, first-person, cover, mount and ragdoll graph paths are retained;
this is not evidence of their runtime acceptance after integration.

`bind_protagonist_shoulder_cameras.py` backs up the two protagonist assets,
checks their owned GameplayCameraComponent templates, and changes only the
CameraReference asset while preserving its other fields. No mission map or
vendor asset is saved. M12 retains SHA256
`B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5`.

## Evidence

- `CameraShoulderDocked-20260920-032436-741e89a9/ShoulderHelper/helper-check.json`
  checks both protagonist classes through explicit targets and claim release.
  These are isolated API checks, not gameplay acceptance.
- The first rendered PIE run, `ShoulderOutput-20260920-042153-0b04c591`, failed:
  the completed graph remained unsaved, so the fresh process loaded the earlier
  director. That failure report is preserved.
- After explicitly saving the compiled director,
  `ShoulderOutputSaved-20260920-042655-dd44ae81/shoulder-runtime.json` passed all
  six checks on the naturally possessed Tarrik. Actual PlayerCameraManager
  output measured +80/-80 cm Far, +60/-60 cm Balanced, +80/-80 cm Close, with
  90-degree FOV. The script releases its temporary claim and ends PIE.
- Full gate `20260920-042906-5bf739c3` passed the editor build and all 719 matching
  automation tests, with 95 warnings and unchanged source integrity. No
  SkipBuild, packaged build, or GPU profiling was used.

Evidence directories are under `Saved/Validation/Aurelion`, except the full
gate under `Saved/Validation`. The reusable rendered check is
`Scripts/Validation/Aurelion/check_shoulder_camera_output.py`.

## Remaining acceptance

The rendered check covers stationary Tarrik in Strafe, using controlled camera
claims. It does not qualify physical input, Selene's live handoff, weapon aiming,
cover/crouch, first-person transitions, corner collision, or the complete mission
route. Both protagonist bindings are implemented; those gameplay checks remain
open. This does not establish 90% TDD alignment or final visual acceptance.
