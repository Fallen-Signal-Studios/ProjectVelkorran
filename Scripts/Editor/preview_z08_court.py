"""Authored court surface with flush margins; native encounter actors retained."""
from pathlib import Path
import json,os,runpy,shutil,time
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);original=list(subsystem.get_all_level_actors());by_label={a.get_actor_label():a for a in original};assert len(original)==3139
source=root/'Art/Source/Aurelion/Z08CourtKit';fit=json.loads((source/'court-fit.json').read_text());helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));before=helpers['snapshot_actor_state'](original)
dest='/Game/Aurelion/Environment/ArchitectureKit';materials={k:dest+'/Materials/M_AurelionKit_'+v for k,v in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_Basalt':'PavingBasalt','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal'}.items()}
mesh=helpers['import_owned_mesh'](json.loads((source/'manifest.json').read_text())['modules'][0],source,dest+'/Meshes',materials)
if globals().get('COMPARE_TRADITIONAL',False):
    assert not globals().get('PERSIST',False)
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);settings=sm.get_nanite_settings(mesh)
    settings.set_editor_property('enabled',False);settings.set_editor_property('fallback_target',unreal.NaniteFallbackTarget.PERCENT_TRIANGLES);settings.set_editor_property('fallback_percent_triangles',1.0);settings.set_editor_property('fallback_relative_error',0.0)
    sm.set_nanite_settings(mesh,settings,True)
c=by_label[fit['old']['actor']].get_component_by_class(unreal.InstancedStaticMeshComponent)
assert [c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]==fit['old']['all_instance_transforms']
c.modify()
for row in sorted(fit['selected'],key=lambda r:r['index'],reverse=True):assert c.remove_instance(row['index'])
for row in fit['underlay']:
    c=by_label[row['actor']].static_mesh_component;c.modify();c.set_visibility(False,False);c.set_hidden_in_game(True,False)
a=subsystem.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*fit['location']));a.set_actor_label('KIT_Z08_IsolationCourt');a.set_folder_path('Aurelion/CustomArchitecture/Z08/Court')
c=a.static_mesh_component;c.set_static_mesh(mesh);c.set_collision_profile_name('NoCollision');c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);a.set_actor_enable_collision(False)
assert helpers['snapshot_actor_state'](original)==before
geometry=runpy.run_path(str(root/'Scripts/Editor/check_z08_court.py'))['check_z08_court'](world,list(subsystem.get_all_level_actors()),nanite_enabled=not globals().get('COMPARE_TRADITIONAL',False))
persist=bool(globals().get('PERSIST',False))
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'z08-court-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',geometry=geometry,preserved_actor_states=len(original)),indent=2))
if not globals().get('SKIP_COURT_CAPTURE',False):
    editor.editor_set_game_view(True)
    capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
    capture=capture.replace("('entry',unreal.Vector(p.x,p.y,p.z+165),unreal.Rotator(yaw=90),90)","('court-entry',unreal.Vector(-500,19800,-950),unreal.Rotator(pitch=-5,yaw=75),90)")
    capture=capture.replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('court-pattern',unreal.Vector(0,19400,-250),unreal.Rotator(pitch=-35,yaw=90),85)").replace('z01-','z08-')
    exec(compile("p=by_label['Z08_Entry_StandIn']"+capture,'z08_court_review','exec'))
