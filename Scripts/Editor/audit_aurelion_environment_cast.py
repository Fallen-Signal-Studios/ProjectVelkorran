"""Read-only inventory of both mission maps, every static mesh and named cast."""
import collections
import json
import os
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY']) / 'environment-cast-audit.json'
assert not out.exists(), 'Preserve prior audits'
editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages(), 'Save or resolve existing edits first'
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
report = dict(scope='Inventory only; no visual or gameplay acceptance', maps={}, cast={}, character_assets=[], errors=[])
registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.wait_for_completion()
for map_name in ('L_Aurelion_M12', 'L_Aurelion_M13'):
    assert editor.load_level('/Game/Aurelion/Maps/' + map_name)
    rows, usage = [], collections.Counter()
    for actor in actors.get_all_level_actors():
        components = []
        for c in actor.get_components_by_class(unreal.StaticMeshComponent):
            mesh = c.get_editor_property('static_mesh')
            if not mesh:
                continue
            count = c.get_instance_count() if isinstance(c, unreal.InstancedStaticMeshComponent) else 1
            path = mesh.get_path_name()
            usage[path] += count
            components.append(dict(mesh=path, instance_count=count,
                visible=c.get_editor_property('visible'), hidden_in_game=c.get_editor_property('hidden_in_game'),
                collision=str(c.get_collision_enabled()), transform=c.get_world_transform().export_text(),
                materials=[c.get_material(i).get_path_name() if c.get_material(i) else None for i in range(c.get_num_materials())]))
        label = actor.get_actor_label()
        if components or label.startswith(('REVIEW_', 'Z')):
            origin, extent = actor.get_actor_bounds(False)
            rows.append(dict(label=label, actor_class=actor.get_class().get_name(),
                hidden=actor.get_editor_property('hidden'), location=actor.get_actor_location().export_text(),
                rotation=actor.get_actor_rotation().export_text(), bounds_origin=origin.export_text(),
                bounds_extent=extent.export_text(), components=components))
    report['maps'][map_name] = dict(actors=rows, unique_meshes=len(usage), mesh_instances=sum(usage.values()),
                                  mesh_usage=dict(usage.most_common()))
for role in ('Tharne', 'Lyessa', 'Lyric', 'Malik'):
    appearance = unreal.load_asset('/Game/Aurelion/Art/Characters/Appearances/CA_Aurelion' + role)
    definition = unreal.load_asset('/Game/Aurelion/Characters/NPC_Aurelion' + role)
    report['cast'][role] = dict(appearance=appearance.get_path_name() if appearance else None,
        attributes=appearance.get_editor_property('character_attributes').export_text() if appearance else None,
        definition_appearance=str(definition.get_editor_property('default_appearance')) if definition else None)
for asset in registry.get_all_assets():
    cls = str(asset.asset_class_path.asset_name)
    if cls in ('MetaHumanCharacter', 'MetaHumanIdentity') or (cls == 'SkeletalMesh' and str(asset.package_name).startswith('/Game/')):
        report['character_assets'].append(dict(path=str(asset.package_name), asset_class=cls))
report['metahuman_editor_available'] = hasattr(unreal, 'MetaHumanCharacterEditorSubsystem')
assert editor.load_level('/Game/Aurelion/Maps/L_Aurelion_M12')
out.write_text(json.dumps(report, indent=2), encoding='utf8')
unreal.log('ENVIRONMENT_CAST_AUDIT_COMPLETE ' + str(out))
