# Owned shoulder rig parameters

Six owned Far/Balanced/Close Aim and Strafe camera rigs now expose a public
Vector3d `ShoulderOffset` parameter targeting the nested third-person `Offset`.
The parameter was created using the Camera Rig editor's Camera Interface
Parameter action; no private compiled variable ID is used as a runtime API.

These assets are an implementation dependency, not completed shoulder swapping.
They are not assigned to live protagonists. The existing owned camera director
and camera-asset drafts remain untracked and are not part of this change.
Director propagation from the resolved camera state, input routing, left/right
presentation, camera-claim precedence and collision acceptance remain open.

| View | Default offset (cm) | Damping forward / lateral / vertical |
|---|---|---|
| Far Aim | -100, 60, 60 | 300 / 300 / 30 |
| Balanced Aim | -100, 60, 60 | 300 / 300 / 30 |
| Close Aim | -100, 60, 60 | 300 / 300 / 30 |
| Far Strafe | -300, 80, 60 | 40 / 40 / 20 |
| Balanced Strafe | -225, 60, 60 | 40 / 40 / 20 |
| Close Strafe | -100, 80, 60 | 30 / 30 / 30 |

The aim wrappers retain their 0.2-second SmootherStep entry blends. The Balanced
Strafe wrapper retains its existing public CrouchOffset interface. All wrappers
still reference the original shared third-person rig, preserving its collision
and damping implementation. Neither vendor assets nor protagonist assignments
were saved. M12 retains its protected SHA256 beginning B7CEEAB5.

Fresh-process readback `ShoulderRigReadback-20260920-033758-3bdb2eca` passed all
six assets. `Scripts/Editor/check_shoulder_rigs_readonly.py` exports owned and
source wrappers, verifies public interface compilation/binding, and compares
default offsets, damping, transition counts/types/durations and crouch defaults.
This is saved-content evidence, not a runtime or visual gameplay test.

Validation `20260920-033909-31d7add4` passed the editor build check (target already
up to date) and all 719 matching automation tests, with source integrity
unchanged. No packaged build or runtime shoulder acceptance was performed.
