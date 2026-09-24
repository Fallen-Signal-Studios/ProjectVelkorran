"""Independently reload/check the saved M13 open canopy, then capture player-eye views."""
from pathlib import Path
import hashlib
import json
import os
import runpy
import unreal

root=Path(unreal.Paths.project_dir())
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
save_dir=root/'Saved/Validation/Aurelion/Z12SparseCanopySave-20260924-013021-4d936c56'
save=json.loads((save_dir/'sparse-canopy-save.json').read_text())
assert save['status']=='saved_requires_fresh_editor_verification'
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Aurelion_M13' and not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
digest=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
maps={name:root/'Content/Aurelion/Maps'/name for name in ('L_Aurelion_M12.umap','L_Aurelion_M13.umap')}
assert {name:digest(path) for name,path in maps.items()}==save['map_hashes_after']
all_actors=list(api.get_all_level_actors())
by_label={a.get_actor_label():a for a in all_actors}
assert len(by_label)==len(all_actors)
roof=save['hidden_roof_labels']
girders=save['new_girder_labels']
assert len(roof)==66 and len(girders)==16
assert all(sum(a.get_actor_label()==name for a in all_actors)==1 for name in roof+girders)
assert all(not by_label[name].static_mesh_component.get_editor_property('visible')
           and by_label[name].static_mesh_component.get_editor_property('hidden_in_game')
           and by_label[name].static_mesh_component.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
           for name in roof)
assert all(by_label[name].static_mesh_component.static_mesh.get_path_name()==save['mesh_path']
           and by_label[name].static_mesh_component.get_editor_property('visible')
           and by_label[name].static_mesh_component.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
           and not by_label[name].static_mesh_component.get_editor_property('can_ever_affect_navigation')
           for name in girders)
for dock,cx in (('Dominion',-3800),('Reformation',3800)):
    for side,x in (('Left',-13.2),('Right',13.2)):
        for i,y in enumerate((-6.225,-2.075,2.075,6.225)):
            label='Aurelion_Z12_%s_OpenCanopyGirder_%s_%d'%(dock,side,i)
            loc=by_label[label].get_actor_location()
            assert max(abs(a-b) for a,b in zip((loc.x,loc.y,loc.z),
                                                 (cx+x*100,47500+y*100,750)))<.1
old_manifest=json.loads((root/'Art/Source/Aurelion/Z12DepartureCanopy/manifest.json').read_text())
plan=runpy.run_path(str(root/'Scripts/Editor/aurelion_z12_canopy_plan.py'))['plan'](old_manifest)
kept=[row['label'] for row in plan if '_Canopy_Foot_' in row['label'] or '_Canopy_Pendant_' in row['label']]
assert len(kept)==36
assert all(by_label[name].static_mesh_component.get_editor_property('visible') and
           by_label[name].static_mesh_component.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
           for name in kept)
assert not any(a.get_actor_label().startswith(('PREVIEW_Aurelion_Z12_Dominion_OpenCanopyGirder_',
                                              'PREVIEW_Aurelion_Z12_Reformation_OpenCanopyGirder_'))
               for a in all_actors)
(out/'sparse-canopy-fresh.json').write_text(json.dumps(dict(
    status='fresh_editor_loaded_pass',hidden_roof=66,new_girders=16,
    visible_original_feet_and_pendants=36,collision_and_nav_disabled=True,
    map_hashes=save['map_hashes_after'],actor_count=len(all_actors)),indent=2))
runpy.run_path(str(root/'Scripts/Editor/preview_m13_route.py'),init_globals={
    'M13_ROUTE_VIEWS':[('west-final-girder',(-900,47200,180)),
                       ('east-final-girder',(900,47800,180))],
    'M13_ROUTE_YAWS':{'west-final-girder':180,'east-final-girder':0},
    'M13_ROUTE_PITCHES':{'west-final-girder':8,'east-final-girder':8},
})
print('M13_SPARSE_CANOPY_FRESH_PASS')
