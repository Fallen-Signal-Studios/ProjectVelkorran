"""Replace capture-gallery roof art in place with fitted stone coffers."""
import json,os,runpy,shutil,time
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);persist=bool(globals().get('PERSIST',False))
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);original=list(subsystem.get_all_level_actors());by_label={a.get_actor_label():a for a in original};assert len(original)==2897
source=root/'Art/Source/Aurelion/Z07CeilingKit';fit=json.loads((source/'ceiling-fit.json').read_text())
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
unchanged=[a for a in original if a.get_actor_label() not in [r['actor'] for r in fit['bands']]];before=helpers['snapshot_actor_state'](unchanged)
row=fit['roof_baseline'];c=by_label[row['actor']].get_component_by_class(unreal.InstancedStaticMeshComponent)
assert c.static_mesh.get_path_name()==row['mesh']
assert [c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]==row['all_instance_transforms']
assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
destination='/Game/Aurelion/Environment/ArchitectureKit'
materials={key:destination+'/Materials/M_AurelionKit_'+value for key,value in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal'}.items()}
spec=json.loads((source/'manifest.json').read_text())['modules'][0];m=helpers['import_owned_mesh'](spec,source,destination+'/Meshes',materials);b=m.get_bounds()
c.modify();c.set_static_mesh(m);c.set_editor_property('override_materials',[])
for row in fit['placements']:
    t=unreal.Transform(location=unreal.Vector(row['x'],row['y'],-300-b.origin.z-b.box_extent.z))
    assert c.update_instance_transform(row['index'],t,True,True,True)
for row in fit['bands']:
    a=by_label[row['actor']];c=a.static_mesh_component
    assert a.get_actor_transform().export_text()==row['actor_transform'] and c.static_mesh.get_path_name()==row['mesh']
    a.modify();c.modify();a.set_actor_enable_collision(False);c.set_collision_profile_name('NoCollision');c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);c.set_visibility(False,False);c.set_hidden_in_game(True,False)
assert helpers['snapshot_actor_state'](unchanged)==before
for row in fit['lights']:
    c=by_label[row['actor']].get_component_by_class(unreal.RectLightComponent);color=c.get_editor_property('light_color')
    assert [color.r,color.g,color.b,color.a]==row['before_rgb8'] and abs(c.intensity-row['before_intensity'])<.01
    assert c.get_world_transform().export_text()==row['transform']
    assert {k:str(c.get_editor_property(k)) for k in row['properties']}==row['properties']
    c.modify();c.set_intensity(row['after_intensity']);c.set_light_color(unreal.LinearColor(*row['after_linear']))
geometry=runpy.run_path(str(root/'Scripts/Editor/check_z07_ceiling.py'))['check_z07_ceiling'](world,original)
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'z07-ceiling-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',geometry=geometry,preserved_actor_states=len(unchanged)),indent=2))
if not globals().get('SKIP_CEILING_CAPTURE',False):
    editor.editor_set_game_view(True)
    capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
    capture=capture.replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('ceiling-detail',unreal.Vector(0,15400,-650),unreal.Rotator(pitch=32,yaw=-90),90)").replace('z01-','z07-')
    exec(compile("p=by_label['Z07_Entry_StandIn']"+capture,'z07_ceiling_capture','exec'))
