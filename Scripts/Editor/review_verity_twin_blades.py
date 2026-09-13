"""Reload and audit the saved Verity bindings; optionally open the actual montage editor."""
import json
import os
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
root = '/Game/Characters/Animation/VerityTwinBlades'
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
ability = unreal.load_asset(root + '/GA_SovVerityTwinAttack')
item = unreal.load_asset('/Game/Items/Weapons/WI_Verity')
assert ability.generated_class() in unreal.get_default_object(item.generated_class()).get_editor_property('weapon_abilities')
combo = unreal.load_asset(root + '/Combo_VerityTwinBlades')
assert list(unreal.get_default_object(ability.generated_class()).get_editor_property('DefaultComboAnimations')) == [combo]
overlay = unreal.load_asset('/Game/Characters/Animation/ABP_SovVerityOverlay')
parent = unreal.load_asset(root + '/ABP_VerityTwinMeleeBase')
parent_tag = str(unreal.AssetRegistryHelpers.get_tag_value(unreal.AssetRegistryHelpers.create_asset_data(overlay), 'ParentClass'))
assert 'ABP_VerityTwinMeleeBase_C' in parent_tag, parent_tag
rows = list(combo.get_editor_property('character_anims'))
assert len(rows) == 4
report = dict(status='saved asset bindings verified', live_gameplay_qualified=False, montages=[])
for row in rows:
    montage = row.get_editor_property('montage3p')
    assert montage.get_path_name().startswith(root)
    assert montage.get_editor_property('skeleton') == overlay.get_editor_property('target_skeleton')
    report['montages'].append(montage.get_path_name())
for asset in [parent, overlay, unreal.load_asset(root + '/BS_VerityTwinStance')] + [row.get_editor_property('montage3p') for row in rows]:
    task = unreal.AssetExportTask()
    for key, value in dict(object=asset, exporter=unreal.ObjectExporterT3D(),
                           filename=str(out / (asset.get_name() + '.t3d')), automated=True,
                           prompt=False, selected=False, replace_identical=False).items():
        task.set_editor_property(key, value)
    assert unreal.Exporter.run_asset_export_task(task)
raw = (out / 'ABP_VerityTwinMeleeBase.t3d').read_bytes()
export = raw.decode('utf-16' if raw.startswith(b'\xff\xfe') else 'utf-8-sig')
assert 'VerityTwinStance' in export and 'BS_VerityTwinStance.BS_VerityTwinStance' in export
report['persistent_stance_reference_verified'] = True
(out / 'verity-reload-review.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
if os.environ.get('SOV_AURELION_ENTRY_KEEP_OPEN') == '1':
    unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).open_editor_for_assets([rows[0].get_editor_property('montage3p')])
