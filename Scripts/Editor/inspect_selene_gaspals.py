"""Read-only compatibility inventory; never assigns or saves animation assets."""
import json
import os
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
registry = unreal.AssetRegistryHelpers.get_asset_registry()
report = {'assets': {}, 'saves': [], 'runtime_qualified': False}
paths = [
    '/Game/PlayerCharacters/BP_SovSelene',
    '/Game/Characters/Definitions/PD_Selene',
    '/NarrativePro/Pro/Core/Character/Biped/Appearances/Mannequin/Appearance_Selene',
    '/NarrativePro/Pro/Core/Character/Biped/Animation/ABP/Base/ABP_Biped',
    '/GASPALS/Blueprints/ABP_SandboxCharacter',
    '/GASPALS/OverlaySystem/Overlays/Bases/Feminine/ABP_OverlayBase_Feminine',
    '/GASPALS/OverlaySystem/Overlays/Bases/Feminine/DA_OverlayBase_Feminine',
    '/GASPALS/OverlaySystem/Overlays/Bases/Feminine/Pose_Feminine_Stand_Move',
    '/GASPALS/OverlaySystem/Overlays/Bases/ABP_OverlayBase_Base',
]
for path in paths:
    asset = unreal.load_asset(path)
    entry = report['assets'][path] = {'loaded': bool(asset)}
    if not asset:
        continue
    entry['class'] = asset.get_class().get_path_name()
    entry['dependencies'] = [str(x) for x in registry.get_dependencies(path, unreal.AssetRegistryDependencyOptions())]
    for label, obj in [('asset', asset), ('defaults', unreal.get_default_object(asset.generated_class()) if isinstance(asset, unreal.Blueprint) else None)]:
        if obj is None:
            continue
        props = entry[label] = {}
        for name in sorted(set(dir(obj)) | {'target_skeleton', 'skeleton', 'mesh', 'animation_set', 'anim_class', 'overlay_base', 'overlay_pose', 'base_pose', '3P_Idle'}):
            if name.startswith('_'):
                continue
            try:
                value = obj.get_editor_property(name)
                props[name] = str(value)
            except Exception:
                pass
        if isinstance(obj, unreal.Character):
            mesh = obj.get_editor_property('mesh')
            entry['mesh'] = {'skeletal_mesh': str(mesh.get_editor_property('skeletal_mesh_asset')), 'anim_class': str(mesh.get_editor_property('anim_class'))}
        if label == 'defaults':
            task = unreal.AssetExportTask()
            task.object = obj
            task.filename = str(out / (asset.get_name() + '_defaults.copy'))
            task.automated = True
            task.prompt = False
            unreal.Exporter.run_asset_export_task(task)
    task = unreal.AssetExportTask()
    task.object = asset
    task.filename = str(out / (asset.get_name() + '.copy'))
    task.automated = True
    task.prompt = False
    entry['text_exported'] = unreal.Exporter.run_asset_export_task(task)
(out / 'selene-gaspals-inventory.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
