"""Replace existing bound capture-base visuals without changing scene targets."""
import json,os,runpy,shutil,time
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);persist=bool(globals().get('PERSIST',False))
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);original=list(subsystem.get_all_level_actors());by_label={a.get_actor_label():a for a in original};assert len(original)==2923
source=root/'Art/Source/Aurelion/Z07CaptureKit';fit=json.loads((source/'capture-fit.json').read_text());helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));before=helpers['snapshot_actor_state'](original)
destination='/Game/Aurelion/Environment/ArchitectureKit';materials={key:destination+'/Materials/M_AurelionKit_'+value for key,value in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal'}.items()}
meshes={s['asset']:helpers['import_owned_mesh'](s,source,destination+'/Meshes',materials) for s in json.loads((source/'manifest.json').read_text())['modules']}
for row in fit['components']:
    if row['instance_count'] is None:continue
    a=by_label[row['actor']];c=a.get_component_by_class(unreal.InstancedStaticMeshComponent)
    assert c.static_mesh.get_path_name()==row['mesh'] and c.get_instance_count()==1 and c.get_instance_transform(0,world_space=True).export_text()==row['all_instance_transforms'][0]
    c.modify()
    if 'Post' in row['actor']:c.set_visibility(False,False);c.set_hidden_in_game(True,False)
    else:
        faction='Dominion' if 'DOMINION' in row['actor'] else 'Reformation';c.set_static_mesh(meshes['SM_Aurelion_KIT_Z07'+faction+'Capture']);c.set_editor_property('override_materials',[]);p=a.get_actor_location()
        assert c.update_instance_transform(0,unreal.Transform(location=unreal.Vector(p.x,p.y,-900)),True,True,True)
assert helpers['snapshot_actor_state'](original)==before
geometry=runpy.run_path(str(root/'Scripts/Editor/check_z07_capture.py'))['check_z07_capture'](world,original)
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'z07-capture-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',geometry=geometry,preserved_actor_states=len(original)),indent=2))
if not globals().get('SKIP_CAPTURE_VIEWS',False):
    editor.editor_set_game_view(True)
    capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
    capture=capture.replace("('entry',unreal.Vector(p.x,p.y,p.z+165),unreal.Rotator(yaw=90),90)","('dominion',unreal.Vector(-450,14600,-520),unreal.Rotator(pitch=-8,yaw=125),75)")
    capture=capture.replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('reformation',unreal.Vector(450,14600,-520),unreal.Rotator(pitch=-8,yaw=55),75)").replace('z01-','z07-')
    exec(compile("p=by_label['Z07_Entry_StandIn']"+capture,'z07_capture_review','exec'))
