"""Guarded preview/save of one custom visual across five native Z11 request actors."""
from pathlib import Path
import hashlib
import json
import os
import runpy
import shutil

import unreal

root = Path(unreal.Paths.project_dir())
base = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
source = root / 'Art/Source/Aurelion/Z11WitnessTablet'
spec = json.loads((source / 'manifest.json').read_text(encoding='utf-8'))
assert json.loads((source / 'verification.json').read_text(encoding='utf-8'))['status'] == 'round_trip_pass'
assert len(spec['modules']) == 1
row = spec['modules'][0]
survey = json.loads((base / 'z11-witness-tablet-survey.json').read_text(encoding='utf-8'))
assert survey['status'] == 'read_only_pass' and len(survey['actors']) == 5
save = os.environ.get('SOV_Z11_TABLET_SAVE') == '1'
review = json.loads(Path(os.environ['SOV_Z11_TABLET_REVIEW']).read_text(encoding='utf-8')) if save else None
out = base / ('Save' if save else 'Preview')
out.mkdir(parents=True, exist_ok=True)

level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_sys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world and world.get_name() == 'L_Aurelion_M13' and not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
maps = {name: root / 'Content/Aurelion/Maps' / name
        for name in ('L_Aurelion_M12.umap','L_Aurelion_M13.umap')}
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
before = {name:digest(p) for name,p in maps.items()}
assert before['L_Aurelion_M13.umap'] == survey['map_sha256']
assert not save or before == review['map_hashes_before']
source_hash = digest(source / (row['asset'] + '.fbx'))
assert not save or source_hash == review['source_fbx_sha256']

actors = list(actor_sys.get_all_level_actors())
by_path = {a.get_path_name():a for a in actors}
targets = [by_path[r['path']] for r in survey['actors']]
assert all(isinstance(a, unreal.SovAurelionRequestActor) for a in targets)
protected = [a for a in actors if a not in targets]
helper = runpy.run_path(str(root / 'Scripts/Editor/aurelion_architecture_helpers.py'))
protected_before = helper['snapshot_actor_state'](protected)

def component(actor,name):
    matches = [c for c in actor.get_components_by_class(unreal.PrimitiveComponent)
               if c.get_name() == name]
    assert len(matches) == 1,(actor.get_actor_label(),name)
    return matches[0]

def item(actor):
    v = component(actor,'Visual')
    b = component(actor,'Body')
    return dict(label=actor.get_actor_label(), path=actor.get_path_name(),
                actor_transform=actor.get_actor_transform().export_text(),
                body_transform=b.get_world_transform().export_text(),
                body_collision=str(b.get_collision_enabled()),
                visual_transform=v.get_world_transform().export_text(),
                visual_collision=str(v.get_collision_enabled()),
                visual_nav=bool(v.get_editor_property('can_ever_affect_navigation')),
                visual_mesh=v.static_mesh.get_path_name(),
                materials=[v.get_material(i).get_path_name() for i in range(v.get_num_materials())])

original = [item(a) for a in targets]
for a,old in zip(targets,original):
    assert old['visual_mesh'].endswith('SM_KB3D_CPI_PropConsoleLarge_A')
    assert old['body_collision'] == str(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
    assert old['visual_collision'] == str(unreal.CollisionEnabled.NO_COLLISION)
    assert a.get_actor_enable_collision()

dest = '/Game/Aurelion/Environment/ArchitectureKit'
materials = {
    'M_Aurelion_IvoryStone':dest+'/Materials/M_AurelionKit_Ivory',
    'M_Aurelion_ObservationBlackStone':dest+'/Materials/M_AurelionKit_ObservationBlackStone',
    'M_Aurelion_ChannelShadow':dest+'/Materials/M_AurelionKit_Reveal',
    'M_Aurelion_AncientGold':dest+'/Materials/M_AurelionKit_Gold',
    'M_Aurelion_QuietDisplay':dest+'/Materials/M_AurelionKit_ViewGlass',
}
mesh = helper['import_owned_mesh'](row,source,dest+'/Meshes',materials)
assert mesh and mesh.get_name() == row['asset']
for actor in targets:
    visual = component(actor,'Visual')
    visual.set_static_mesh(mesh)
    for i in range(visual.get_num_materials()):
        visual.set_material(i,mesh.get_material(i))

updated = [item(a) for a in targets]
for old,new,actor in zip(original,updated,targets):
    for key in ('label','path','actor_transform','body_transform','body_collision',
                'visual_transform','visual_collision','visual_nav'):
        assert old[key] == new[key],(actor.get_actor_label(),key)
    assert new['visual_mesh'] == mesh.get_path_name()
    assert new['materials'] == [mesh.get_material(i).get_path_name() for i in range(len(new['materials']))]
assert helper['snapshot_actor_state'](protected) == protected_before
assert {name:digest(p) for name,p in maps.items()} == before

report = dict(status='unsaved_preview',map_hashes_before=before,
              source_fbx_sha256=source_hash,original_actor_count=len(actors),
              protected_actors_unchanged=True,actor_and_body_state_unchanged=True,
              original=original,actors_after=updated,
              authored_collision_hulls=row['convex_hulls'])
if save:
    assert updated == review['actors_after']
    shutil.copy2(maps['L_Aurelion_M13.umap'], out/'L_Aurelion_M13.before.umap')
    assert level.save_current_level() and level.load_level('/Game/Aurelion/Maps/L_Aurelion_M13')
    restored = {a.get_actor_label():a for a in actor_sys.get_all_level_actors()}
    for expected in updated:
        actual = item(restored[expected['label']])
        assert actual == expected,expected['label']
    assert digest(maps['L_Aurelion_M12.umap']) == before['L_Aurelion_M12.umap']
    report.update(status='saved_reloaded',map_hash_after=digest(maps['L_Aurelion_M13.umap']))

(out / 'z11-witness-tablet-fit.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
previous = os.environ['SOV_AURELION_RUN_DIRECTORY']
try:
    os.environ['SOV_AURELION_RUN_DIRECTORY'] = str(out)
    runpy.run_path(str(root / 'Scripts/Editor/preview_m13_route.py'),init_globals={
        'ALLOW_DIRTY_PREVIEW':not save,
        'M13_ROUTE_VIEWS':[
            ('witness-room',(750,43100,190)),
            ('witness-near',(100,42830,155)),
            ('witness-close',(360,43330,155)),
        ],
        'M13_ROUTE_YAWS':{'witness-room':180,'witness-near':180,'witness-close':180},
        'M13_ROUTE_PITCHES':{'witness-room':-8,'witness-near':-10,'witness-close':-11},
    })
finally:
    os.environ['SOV_AURELION_RUN_DIRECTORY'] = previous
print('M13_Z11_WITNESS_TABLETS_'+('SAVED' if save else 'UNSAVED_PREVIEW')+'_PASS',out)
