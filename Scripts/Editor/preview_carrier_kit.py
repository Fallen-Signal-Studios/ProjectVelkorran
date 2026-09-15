"""Replace only the carrier visual HISMs, preserving their animated parents."""
from pathlib import Path
import json, os, runpy, shutil, unreal
root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/CarrierKit';out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
old=json.loads((source/'carrier-baseline.json').read_text());manifest=json.loads((source/'manifest.json').read_text())
helper=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));check=runpy.run_path(str(root/'Scripts/Editor/check_carrier_kit.py'))
labels={row['label'] for row in old['parts']}
legacy=json.loads((source/'legacy-spire-baseline.json').read_text())
untouched=[a for a in actors if a.get_actor_label() not in labels and a.get_actor_label()!=legacy['actor']]
before=helper['snapshot_actor_state'](untouched)
dest='/Game/Aurelion/Environment/ArchitectureKit';persist=bool(globals().get('PERSIST_CARRIER_KIT',False))
materials={k:dest+'/Materials/M_AurelionKit_'+v for k,v in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal'}.items()}
meshes={spec['asset']:unreal.load_asset(dest+'/Meshes/'+spec['asset']) if persist else helper['import_owned_mesh'](spec,source,dest+'/Meshes',materials) for spec in manifest['modules']}
for row in old['parts']:
    a=next(a for a in actors if a.get_actor_label()==row['label'])
    c=a.get_component_by_class(unreal.InstancedStaticMeshComponent)
    oldc=next(c0 for c0 in row['components'] if c0['instances'] is not None)
    assert c.static_mesh.get_path_name()==oldc['mesh']
    assert [c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]==oldc['instances']
    c.modify();c.clear_instances();c.set_static_mesh(meshes[check['mesh_name'](row['label'])]);c.set_editor_property('override_materials',[])
    c.set_collision_profile_name('NoCollision');c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    c.set_editor_property('can_ever_affect_navigation',False)
    c.add_instance(check['art_transform'](row['label'],{actor.get_actor_label():actor for actor in actors}),world_space=True)
spire=next(a for a in actors if a.get_actor_label()==legacy['actor']);c=spire.static_mesh_component
assert spire.get_actor_transform().export_text()==legacy['actor_transform'] and c.static_mesh.get_path_name()==legacy['mesh']
assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
c.modify();c.set_visibility(False);c.set_hidden_in_game(True)
assert helper['snapshot_actor_state'](untouched)==before
settings=check['check_carrier_kit'](actors)
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap')
    assert editor.save_current_level()
(out/'carrier-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',settings=settings),indent=2))
overlaps=[]
for candidate in untouched:
    if candidate.get_editor_property('hidden'):continue
    for component in candidate.get_components_by_class(unreal.StaticMeshComponent):
        if not component.static_mesh or not component.get_editor_property('visible') or component.get_editor_property('hidden_in_game'):continue
        origin,extent,_=unreal.SystemLibrary.get_component_bounds(component)
        lo=[origin.x-extent.x,origin.y-extent.y,origin.z-extent.z];hi=[origin.x+extent.x,origin.y+extent.y,origin.z+extent.z]
        if all(lo[i]<[13200,5750,1000][i] and hi[i]>[8800,-4750,-2500][i] for i in range(3)):
            overlaps.append(dict(label=candidate.get_actor_label(),component=component.get_path_name(),mesh=component.static_mesh.get_path_name(),bounds=[lo,hi],collision=str(component.get_collision_enabled())))
(out/'carrier-context-overlaps.json').write_text(json.dumps(overlaps,indent=2))
capture=(root/'Scripts/Editor/review_eclipse_wall_scars.py').read_text()
prefix,suffix=capture.split('views=[',1)[0],capture.split('state=dict',1)[1]
views="views=[('suspended-route',(6000,-2500,350),(0,30),90),('suspended-wide',(1000,-8000,4500),(-18,42),75),('stable-wide',(1000,-8000,4500),(-18,42),75),('stable-forward',(6500,10000,2000),(-12,-55),75)]\n"
carrier_visuals=[(a.get_actor_label(),a.get_component_by_class(unreal.InstancedStaticMeshComponent)) for a in actors if a.get_actor_label() in labels]
# Editor-only phase isolation for review. Restore both authored visibility values
# before exiting; no journal event or map save is used to stage these pictures.
suffix=suffix.replace("if state['phase']==0:","if state['phase']==0:\n            for label,component in carrier_visuals: component.set_visibility(('Stable' in label)==name.startswith('stable-'))")
suffix=suffix.replace('restore_capture_settings();editor.eject_pilot_level_actor();',"for label,component in carrier_visuals: component.set_visibility(True)\n            restore_capture_settings();editor.eject_pilot_level_actor();")
exec(compile(prefix+views+'state=dict'+suffix,'carrier_capture','exec'),globals())
