"""Fit custom entrances, preserving the climb art and retiring spanning decorative bands."""
import json,os,runpy,shutil,time
from pathlib import Path
from collections import Counter
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);persist=bool(globals().get('PERSIST',False))
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);original=list(subsystem.get_all_level_actors());by_label={a.get_actor_label():a for a in original};assert len(original)==2813
fit=json.loads((root/'Art/Source/Aurelion/Z06EndwallFit/endwall-fit.json').read_text());helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));checks=runpy.run_path(str(root/'Scripts/Editor/check_z06_endwalls.py'))
excluded={r['actor'] for r in fit['bands']};unchanged=[a for a in original if a.get_actor_label() not in excluded];before=helpers['snapshot_actor_state'](unchanged)
baseline=checks['aperture_checks'](world,original,fit,False);(out/'original-aperture.json').write_text(json.dumps(baseline,indent=2))
c=by_label[fit['wall_actor']].get_component_by_class(unreal.InstancedStaticMeshComponent);transforms=[c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]
assert sorted(transforms)==sorted(fit['before_transforms']);selected=Counter(r['transform'] for r in fit['selected']);indices=[]
for i,t in enumerate(transforms):
    if selected[t]>0:indices.append(i);selected[t]-=1
assert len(indices)==52 and not any(selected.values());c.modify()
for i in reversed(indices):assert c.remove_instance(i)
ivory=unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_PavingIvory');assert ivory
for side,face,sign,yaw in [('South',6026,1,0),('North',11574,-1,180)]:
    for row in fit['layout']:
        m=unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Meshes/SM_Aurelion_KIT_'+row['suffix']);assert m
        b=m.get_bounds();a=subsystem.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(row['x'],face-sign*(b.origin.y+b.box_extent.y),-600),unreal.Rotator(yaw=yaw));a.set_actor_label(f'KIT_Z06_End_{side}_{row["name"]}');a.set_folder_path('Aurelion/CustomArchitecture/Z06/EndWalls')
        nc=a.static_mesh_component;nc.set_static_mesh(m);nc.set_collision_profile_name('NoCollision');nc.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);a.set_actor_enable_collision(False)
        for i,slot in enumerate(m.get_editor_property('static_materials')):
            if str(slot.get_editor_property('imported_material_slot_name'))=='M_Aurelion_IvoryStone':nc.set_material(i,ivory)
for row in fit['bands']:
    a=by_label[row['actor']];c=a.static_mesh_component;assert a.get_actor_transform().export_text()==row['actor_transform'] and c.static_mesh.get_path_name()==row['mesh']
    a.modify();c.modify();a.set_actor_enable_collision(False);c.set_collision_profile_name('NoCollision');c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);c.set_visibility(False,False);c.set_hidden_in_game(True,False)
assert helpers['snapshot_actor_state'](unchanged)==before
geometry=checks['check_z06_endwalls'](world,list(subsystem.get_all_level_actors()))
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'z06-endwall-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',geometry=geometry,original_aperture=baseline,preserved_actor_states=len(unchanged)),indent=2))
editor.editor_set_game_view(True)
capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
capture=capture.replace("('entry',unreal.Vector(p.x,p.y,p.z+165),unreal.Rotator(yaw=90),90)","('south-portal',unreal.Vector(900,7250,-435),unreal.Rotator(pitch=12,yaw=-125),90)").replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('north-portal',unreal.Vector(-900,10300,-435),unreal.Rotator(pitch=12,yaw=55),90)").replace('z01-','z06-')
exec(compile("p=by_label['Z06_Entry_StandIn']"+capture,'z06_endwall_capture','exec'))
