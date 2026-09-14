"""Read-only hand trajectories to locate the cinematic throw action window."""
import json
import os
from pathlib import Path
import unreal
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
clip = unreal.load_asset('/NarrativePro/Pro/Demo/Cinematics/TestAnims/Throw_ue5')
rows = []
for frame in range(195):
    pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(clip, frame / 30., unreal.AnimPoseEvaluationOptions())
    row = dict(frame=frame, time=frame / 30.)
    for bone in ['root', 'hand_r', 'hand_l', 'head']:
        location = unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD).translation
        row[bone] = [location.x, location.y, location.z]
    rows.append(row)
(out / 'throw-motion.json').write_text(json.dumps(rows, indent=2), encoding='utf-8')
