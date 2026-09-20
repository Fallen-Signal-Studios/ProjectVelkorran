"""Reload and evaluate authored posture deltas; this is not gameplay acceptance."""
import json
import math
import os
import traceback
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
root = '/Game/Characters/Animation/SeleneGASPALS'
report = dict(status='checking', states={}, gameplay_qualified=False)
try:
    options = unreal.AnimPoseEvaluationOptions()
    options.set_editor_property('retrieve_additive_as_full_pose', True)
    for state in ('Stand_Idle', 'Stand_Move', 'Crouch_Idle', 'Crouch_Move'):
        delta = unreal.load_asset(root + '/A_SeleneFeminineDelta_' + state)
        reference = unreal.load_asset(root + '/SovSelene_Pose_Neutral_' + state)
        feminine = 'Crouch' if state.startswith('Crouch') else state
        target = unreal.load_asset(root + '/SovSelene_Pose_Feminine_' + feminine)
        assert delta and reference and target
        assert delta.get_editor_property('ref_pose_seq') == reference
        assert delta.get_editor_property('additive_anim_type') == unreal.AdditiveAnimationType.AAT_LOCAL_SPACE_BASE
        assert delta.get_editor_property('ref_pose_type') == unreal.AdditiveBasePoseType.ABPT_ANIM_FRAME
        reconstructed = unreal.AnimPoseExtensions.get_anim_pose_at_time(delta, 0, options)
        intended = unreal.AnimPoseExtensions.get_anim_pose_at_time(target, 0, options)
        assert unreal.AnimPoseExtensions.is_valid(reconstructed)
        assert unreal.AnimPoseExtensions.is_valid(intended)
        assert not unreal.AnimPoseExtensions.get_curve_names(reconstructed)
        errors = []
        for bone in unreal.AnimPoseExtensions.get_bone_names(intended):
            a = unreal.AnimPoseExtensions.get_bone_pose(reconstructed, bone, unreal.AnimPoseSpaces.WORLD)
            b = unreal.AnimPoseExtensions.get_bone_pose(intended, bone, unreal.AnimPoseSpaces.WORLD)
            distance = (a.translation - b.translation).length()
            qa, qb = a.rotation, b.rotation
            dot = abs(qa.x * qb.x + qa.y * qb.y + qa.z * qb.z + qa.w * qb.w)
            angle = math.degrees(2 * math.acos(min(1.0, dot)))
            assert math.isfinite(distance) and math.isfinite(angle)
            errors.append((distance, angle))
        maximum_distance = max(e[0] for e in errors)
        maximum_angle = max(e[1] for e in errors)
        report['states'][state] = dict(bones=len(errors), max_position_error_cm=maximum_distance,
                                        max_rotation_error_degrees=maximum_angle)
        assert maximum_distance < 0.1 and maximum_angle < 0.1, report['states'][state]
    blend = unreal.load_asset(root + '/BS_SeleneFemininePostureDelta')
    samples = list(blend.get_editor_property('sample_data'))
    assert len(samples) == 4
    for sample in samples:
        assert sample.get_editor_property('animation').get_editor_property('additive_anim_type') == unreal.AdditiveAnimationType.AAT_LOCAL_SPACE_BASE
    task = unreal.AssetExportTask()
    task.object = blend
    task.filename = str(out / 'selene-posture-blend.copy')
    task.automated = True
    task.prompt = False
    assert unreal.Exporter.run_asset_export_task(task)
    raw = Path(task.filename).read_bytes()
    exported = raw.decode('utf-16' if raw.startswith((b'\xff\xfe', b'\xfe\xff')) else 'utf-8-sig')
    data = next(line for line in exported.splitlines() if 'BlendSpaceData=' in line)
    assert data.count('SampleIndices[0]=') == 2, 'Expected two runtime interpolation triangles'
    report['runtime_interpolation_triangles'] = 2
    report['status'] = 'passed; additive reference reconstruction only'
except Exception:
    report.update(status='failed', error=traceback.format_exc())
    raise
finally:
    (out / 'selene-delta-evaluation.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
