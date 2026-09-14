"""Reload saved cast bindings and export contracts; open source throw for visual trim review."""
import json
import os
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
root = '/Game/Characters/Animation/ProtagonistCasts/'
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
report = dict(abilities=[], count=0, live_qualified=False)
for hero, names in [('Tarrik', ['CinderStickyGrenade', 'VelkorransHunger', 'CinderSlam', 'CinderJudgement', 'CinderlineRequiem']),
                    ('Selene', ['StillpointGrenade', 'VeritysWake', 'StaccatoZero', 'AxiomNullPulse', 'Dispatch'])]:
    for name in names:
        bp = unreal.load_asset('/Game/Abilities/' + hero + '/GA_' + hero + '_' + name)
        cdo = unreal.get_default_object(bp.generated_class())
        pair = list(cdo.get_editor_property('cast_montages'))
        assert len(pair) == 2 and pair[0] != pair[1]
        row = dict(ability=bp.get_path_name(), pair=[])
        for i, montage in enumerate(pair):
            suffix = hero + '_' + name + '_' + ('A' if i == 0 else 'B')
            assert montage.get_path_name().startswith(root + 'AM_' + suffix + '.')
            clip = unreal.load_asset(root + 'AS_' + suffix)
            assert clip.get_editor_property('skeleton') == montage.get_editor_property('skeleton')
            assert not clip.get_editor_property('enable_root_motion')
            assert clip.get_editor_property('force_root_lock')
            assert not unreal.AnimationLibrary.get_animation_notify_events(clip)
            assert not unreal.AnimationLibrary.get_animation_notify_events(montage)
            task = unreal.AssetExportTask()
            for key, value in dict(object=montage, exporter=unreal.ObjectExporterT3D(),
                    filename=str(out / (montage.get_name() + '.t3d')), automated=True,
                    prompt=False, selected=False, replace_identical=False).items():
                task.set_editor_property(key, value)
            assert unreal.Exporter.run_asset_export_task(task)
            row['pair'].append(montage.get_path_name())
            report['count'] += 1
        report['abilities'].append(row)
report['status'] = 'twenty saved cast bindings verified'
(out / 'cast-pair-reload.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
if os.environ.get('SOV_AURELION_ENTRY_KEEP_OPEN') == '1':
    asset = unreal.load_asset('/NarrativePro/Pro/Demo/Cinematics/TestAnims/Throw_ue5')
    unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).open_editor_for_assets([asset])
