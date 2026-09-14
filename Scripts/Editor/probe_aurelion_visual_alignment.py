"""Read actual authored appearances and installed animation assets; never save assets."""
import json
import os
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
report = {'read_only': True, 'appearances': {}, 'definitions': {}, 'pack_assets': [], 'errors': []}
registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.wait_for_completion()
for role in ('Enforcer', 'Linkbound', 'WallRunner', 'Weaver', 'Elite', 'SecurityDrone', 'ContaminatedDrone'):
    name = '/Game/Aurelion/Enemies/NPC_Aurelion' + role
    try:
        definition = unreal.load_asset(name)
        appearance = definition.get_editor_property('default_appearance')
        if not isinstance(appearance, unreal.Object):
            import re
            appearance = unreal.load_asset(re.search(r'(/[^\s\"\']+)', str(appearance)).group(1))
        report['definitions'][role] = {k: str(definition.get_editor_property(k)) for k in
            ('default_factions', 'default_item_loadout', 'ability_configuration', 'activity_configuration')}
        report['appearances'][role] = {'path': appearance.get_path_name(),
            'attributes': appearance.get_editor_property('character_attributes').export_text()}
    except Exception as error:
        report['errors'].append(role + ': ' + repr(error))
for root in ('/Game/Parasites_Pack', '/Game/Aurelion/Enemies/Animation'):
    for asset in registry.get_assets_by_path(root, recursive=True):
        cls = str(asset.asset_class_path.asset_name)
        if cls in ('SkeletalMesh', 'Skeleton', 'AnimBlueprint', 'AnimSequence', 'AnimMontage', 'BlendSpace', 'BlendSpace1D'):
            row = {'path': str(asset.package_name), 'class': cls}
            for tag in ('Skeleton', 'TargetSkeleton', 'ParentClass'):
                row[tag] = str(asset.get_tag_value(tag))
            report['pack_assets'].append(row)
(out / 'visual-alignment.json').write_text(json.dumps(report, indent=2), encoding='utf8')
unreal.log('AURELION_VISUAL_PROBE_COMPLETE ' + str(out))
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
