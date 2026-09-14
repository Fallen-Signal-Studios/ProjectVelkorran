"""Read-only inventory of the Verity overlay and Twin Blades animation contracts."""
import json
import os
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
paths = ['/Game/Characters/Animation/ABP_SovVerityOverlay',
         '/Game/Items/Weapons/WI_Verity', '/Game/Weapons/Visuals/BP_SovVerityWeaponVisual']
paths += ['/NarrativePro/Pro/Core/Character/Biped/Animation/ABP/Overlays/Weapons/ABP_Biped_Overlay_Melee',
          '/NarrativePro/Pro/Demo/Items/Examples/Items/Weapons/Melee/Abilities/GA_Attack_Melee_Verity',
          '/NarrativePro/Pro/Core/Character/Biped/Animation/Sequences/Weapon/Sword/1H/Combo_1H_Sword_Attack',
          '/NarrativePro/Pro/Core/Character/Biped/Animation/Sequences/Weapon/Sword/1H/3P/AM_Sword_3P_1H_Attack_1',
          '/NarrativePro/Pro/Core/Character/Biped/Art/Mannequin/Meshes/SK_Mannequin_Narrative',
          '/Game/TwinBladesBundle/Demo/Characters/Mannequins/Meshes/SK_Mannequin']
registry = unreal.AssetRegistryHelpers.get_asset_registry()
paths += [str(a.package_name) for a in registry.get_assets_by_path(
    '/Game/TwinBladesBundle/TwinBladesAndTwinSword/TwinbladesBase/Animation', True)]
report = []
for path in paths:
    asset = unreal.load_asset(path)
    assert asset, path
    objects = [asset]
    if isinstance(asset, unreal.Blueprint):
        objects.append(unreal.get_default_object(asset.generated_class()))
    entry = {'path': path, 'class': asset.get_class().get_name()}
    if isinstance(asset, unreal.Blueprint):
        entry['defaults'] = {}
        for field in ('DefaultComboAnimations', 'DefaultDualWieldComboAnimations',
                      'DefaultShieldComboAnimations', 'weapon_abilities'):
            try:
                entry['defaults'][field] = str(objects[-1].get_editor_property(field))
            except Exception as exc:
                entry['defaults'][field] = str(exc)
    for field in ('skeleton', 'sequence_length', 'enable_root_motion', 'target_skeleton'):
        try:
            entry[field] = str(asset.get_editor_property(field))
        except Exception:
            pass
    report.append(entry)
    for obj in objects:
        task = unreal.AssetExportTask()
        for k, v in dict(object=obj, exporter=unreal.ObjectExporterT3D(),
                         filename=str(out / (obj.get_name() + '.t3d')), automated=True,
                         prompt=False, selected=False, replace_identical=False).items():
            task.set_editor_property(k, v)
        assert unreal.Exporter.run_asset_export_task(task), path
(out / 'animation-inventory.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
unreal.log('VERITY_TWIN_BLADES_INSPECTION_COMPLETE')
