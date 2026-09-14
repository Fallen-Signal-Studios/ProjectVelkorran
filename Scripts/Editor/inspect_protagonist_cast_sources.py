"""Read-only source clips and native ability presentation defaults."""
import json
import os
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
registry = unreal.AssetRegistryHelpers.get_asset_registry()
roots = [
    '/NarrativePro/Pro/Core/Character/Biped/Animation/Sequences/Weapon',
    '/Game/Anims/CombatMasterBundle/Animations/DynamicGreatSword/RootMotion/Manny_UE5/Skill',
    '/Game/TwinBladesBundle/TwinBladesAndTwinSword/TwinbladesBase/Animation/Attack',
    '/Game/Abilities/Tarrik', '/Game/Abilities/Selene',
]
rows = []
for root in roots:
    for data in registry.get_assets_by_path(root, True):
        cls = str(data.asset_class_path.asset_name)
        if cls not in ('AnimSequence', 'Blueprint'):
            continue
        if cls == 'AnimSequence' and not any(k in str(data.package_name) for k in (
                'Grenade', 'Throw', 'Cast', 'Skill', 'Attack', 'Swipe', 'Melee')):
            continue
        try:
            asset = data.get_asset()
        except Exception as exc:
            rows.append(dict(path=str(data.package_name), error=str(exc)))
            continue
        row = dict(path=str(data.package_name), cls=cls)
        if cls == 'AnimSequence':
            row.update(skeleton=str(asset.get_editor_property('skeleton')),
                       length=asset.get_editor_property('sequence_length'),
                       additive=str(asset.get_editor_property('additive_anim_type')))
        else:
            cdo = unreal.get_default_object(asset.generated_class())
            for prop in ('payload_release_delay', 'post_release_recovery', 'maximum_active_duration'):
                try:
                    row[prop] = cdo.get_editor_property(prop)
                except Exception:
                    pass
        rows.append(row)
(out / 'cast-sources.json').write_text(json.dumps(rows, indent=2), encoding='utf-8')
