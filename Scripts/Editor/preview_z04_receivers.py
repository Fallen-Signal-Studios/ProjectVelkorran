"""Fit custom receiver visuals without changing native interaction or journal state."""
from pathlib import Path
import json,os,runpy,shutil,unreal
root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/Z04ReceiverKit';out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());labels={a.get_actor_label():a for a in actors}
old=json.loads((source/'devices-baseline.json').read_text());spec=json.loads((source/'manifest.json').read_text())['modules'][0]
changed={r['actor'] for r in old['devices'] if not r['actor'].startswith('Aurelion_Z03_')}
helper=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));untouched=[a for a in actors if a.get_actor_label() not in changed];before=helper['snapshot_actor_state'](untouched)
assert not json.loads((source/'coplanar.json').read_text())['overlaps']
dest='/Game/Aurelion/Environment/ArchitectureKit';persist=bool(globals().get('PERSIST_Z04_RECEIVERS',False))
runpy.run_path(str(root/'Scripts/Editor/prepare_z04_receiver_material.py'))['prepare_receiver_material']()
materials={k:dest+'/Materials/M_AurelionKit_'+v for k,v in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal','M_Aurelion_ReceiverStatus':'ReceiverStatus'}.items()}
mesh=unreal.load_asset(dest+'/Meshes/'+spec['asset']) if persist else helper['import_owned_mesh'](spec,source,dest+'/Meshes',materials)
for row in old['devices']:
    if row['actor'] not in changed:continue
    a=labels[row['actor']]
    if row['actor'].startswith('Aurelion_Art'):
        r=next(c for c in row['components'] if 'instances' in c);c=a.get_component_by_class(unreal.InstancedStaticMeshComponent)
        assert [c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]==r['instances']
        keep=c.get_instance_transform(2,world_space=True);c.modify();c.clear_instances();c.add_instance(keep,world_space=True)
    else:
        c=a.visual;r=next(c for c in row['components'] if c['name']=='Visual');assert c.static_mesh.get_path_name()==r['mesh'] and c.get_world_transform().export_text()==r['transform']
        c.modify();c.set_static_mesh(mesh);c.set_editor_property('override_materials',[]);c.set_relative_transform(unreal.Transform(location=unreal.Vector(0,0,-66),rotation=unreal.Rotator(yaw=-90),scale=unreal.Vector(1,1,1)),False,True);c.set_collision_profile_name('NoCollision');c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
assert helper['snapshot_actor_state'](untouched)==before
result=runpy.run_path(str(root/'Scripts/Editor/check_z04_receivers.py'))['check_z04_receivers'](actors)
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'z04-receivers-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',settings=result),indent=2))
# Cosmetic shader-state comparison in the stopped editor; never a gameplay receipt.
labels['Aurelion_E2_ReceiverWest'].visual.set_custom_primitive_data_float(0,1)
capture=(root/'Scripts/Editor/review_eclipse_wall_scars.py').read_text();prefix=capture.split('views=[',1)[0];suffix=capture.split('state=dict',1)[1]
suffix=suffix.replace('def restore_capture_settings():',"def restore_capture_settings():\n    labels['Aurelion_E2_ReceiverWest'].visual.set_custom_primitive_data_float(0,0)")
views="views=[('east-active',(7500,-11450,150),(-9,105),65),('west-shader-disabled',(5600,-10550,150),(-9,105),65),('east-context',(7050,-11700,165),(0,60),85)]\n"
exec(compile(prefix+views+'state=dict'+suffix,'z04_receiver_review','exec'),globals())
