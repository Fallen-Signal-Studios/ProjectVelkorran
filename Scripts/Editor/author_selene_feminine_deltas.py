"""Author unbound posture deltas; never change a character or animation graph."""
import json
import os
import traceback
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
root = '/Game/Characters/Animation/SeleneGASPALS'
report = dict(status='preflight', saved=[], gameplay_bound=False)
states = ('Stand_Idle', 'Stand_Move', 'Crouch_Idle', 'Crouch_Move')
try:
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
    names = ['A_SeleneFeminineDelta_' + state for state in states]
    names.append('BS_SeleneFemininePostureDelta')
    assert all(not unreal.EditorAssetLibrary.does_asset_exist(root + '/' + name) for name in names)
    clips = []
    for state, name in zip(states, names):
        feminine = 'Crouch' if state.startswith('Crouch') else state
        source_path = root + '/SovSelene_Pose_Feminine_' + feminine
        reference = unreal.load_asset(root + '/SovSelene_Pose_Neutral_' + state)
        source = unreal.load_asset(source_path)
        assert source and reference
        assert source.get_editor_property('skeleton') == reference.get_editor_property('skeleton')
        clip = unreal.EditorAssetLibrary.duplicate_asset(source_path, root + '/' + name)
        assert clip
        # ALS layer-weight curves belong to the main graph, not a posture delta.
        unreal.AnimationLibrary.remove_all_curve_data(clip)
        clip.set_editor_property('additive_anim_type', unreal.AdditiveAnimationType.AAT_LOCAL_SPACE_BASE)
        clip.set_editor_property('ref_pose_type', unreal.AdditiveBasePoseType.ABPT_ANIM_FRAME)
        clip.set_editor_property('ref_frame_index', 0)
        # Unreal clears RefPoseSeq while additive mode is still AAT_None.
        clip.set_editor_property('ref_pose_seq', reference)
        assert clip.get_editor_property('ref_pose_seq') == reference
        clip.set_editor_property('enable_root_motion', False)
        clips.append(clip)
    factory = unreal.BlendSpaceFactoryNew()
    factory.set_editor_property('target_skeleton', clips[0].get_editor_property('skeleton'))
    blend = unreal.AssetToolsHelpers.get_asset_tools().create_asset(names[-1], root, unreal.BlendSpace, factory)
    axes = list(blend.get_editor_property('blend_parameters'))
    for index, label, maximum in [(0, 'Speed', 100.0), (1, 'CrouchingAmount', 1.0)]:
        axes[index].set_editor_property('display_name', label)
        axes[index].set_editor_property('min', 0.0)
        axes[index].set_editor_property('max', maximum)
        axes[index].set_editor_property('grid_num', 4)
        axes[index].set_editor_property('wrap_input', False)
    blend.set_editor_property('blend_parameters', axes)
    samples = []
    for clip, (speed, crouch) in zip(clips, [(0, 0), (100, 0), (0, 1), (100, 1)]):
        sample = unreal.BlendSample()
        sample.set_editor_property('animation', clip)
        sample.set_editor_property('sample_value', unreal.Vector(speed, crouch, 0))
        sample.set_editor_property('use_single_frame_for_blending', True)
        sample.set_editor_property('frame_index_to_sample', 0)
        samples.append(sample)
    blend.set_editor_property('sample_data', samples)
    # SBlendSpaceEditor constructs the runtime triangulation. Merely assigning
    # SampleData through reflection does not call UBlendSpace::ResampleData.
    editor = unreal.get_editor_subsystem(unreal.AssetEditorSubsystem)
    assert editor.open_editor_for_assets([blend])
    report['samples'] = [
        {'animation': s.get_editor_property('animation').get_path_name(),
         'coordinates': str(s.get_editor_property('sample_value'))}
        for s in blend.get_editor_property('sample_data')]
    assert len(report['samples']) == 4
    for asset in clips + [blend]:
        assert unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
        report['saved'].append(asset.get_path_name())
    editor.close_all_editors_for_asset(blend)
    report['status'] = 'saved; unbound additive posture content only'
except Exception:
    report.update(status='failed', error=traceback.format_exc())
    raise
finally:
    (out / 'selene-feminine-deltas.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
