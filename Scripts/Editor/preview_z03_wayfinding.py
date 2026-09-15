"""Fit custom floor registers and a mounted destination sign; retire obsolete decor."""
from pathlib import Path
import json,os,runpy,shutil,unreal
root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/Z03WayfindingKit';out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());labels={a.get_actor_label():a for a in actors}
fit=json.loads((source/'presentation-baseline.json').read_text());manifest=json.loads((source/'manifest.json').read_text());old=json.loads((source/'guidance-baseline.json').read_text())
helper=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));changed={r['actor'] for r in fit['decorations']}|{old['actor'],'Aurelion_Art_Sign_Z03_5250c2'}
assert not json.loads((source/'coplanar.json').read_text())['overlaps'] and not json.loads((source/'plaque-coplanar.json').read_text())['overlaps']
untouched=[a for a in actors if a.get_actor_label() not in changed];before=helper['snapshot_actor_state'](untouched)
persist=bool(globals().get('PERSIST_Z03_WAYFINDING',False));dest='/Game/Aurelion/Environment/ArchitectureKit'
materials={k:dest+'/Materials/M_AurelionKit_'+v for k,v in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal','M_Aurelion_UplightLens':'UplightLens'}.items()}
meshes={s['asset']:unreal.load_asset(dest+'/Meshes/'+s['asset']) if persist else helper['import_owned_mesh'](s,source,dest+'/Meshes',materials) for s in manifest['modules']}
for row in fit['decorations']:
    c=labels[row['actor']].get_component_by_class(unreal.StaticMeshComponent)
    assert c.get_world_transform().export_text()==row['transform'] and str(c.get_collision_enabled())==row['collision'] and not c.get_editor_property('hidden_in_game')
    c.modify();c.set_hidden_in_game(True,False);c.set_collision_profile_name('NoCollision');c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
for row in fit['components']:
    if row['class_name']=='TextRenderComponent' and row['actor'].startswith('Z03_'):
        c=labels[row['actor']].get_component_by_class(unreal.TextRenderComponent);assert str(c.text)==row['properties']['text']
        c.modify();c.set_hidden_in_game(True,False)
c=labels[old['actor']].get_component_by_class(unreal.InstancedStaticMeshComponent)
assert c.static_mesh.get_path_name()==old['mesh'] and [c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]==old['all_instance_transforms']
c.modify();c.clear_instances();c.set_static_mesh(meshes['SM_Aurelion_KIT_Z03RouteRegister']);c.set_editor_property('override_materials',[]);c.set_collision_profile_name('NoCollision');c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
for row in manifest['placements']:c.add_instance(unreal.Transform(location=unreal.Vector(*row['location_cm']),rotation=unreal.Rotator(),scale=unreal.Vector(1,1,1)),world_space=True)
a=labels['Aurelion_Art_Sign_Z03_5250c2'];a.modify();a.set_actor_location_and_rotation(unreal.Vector(7600,-15051,242.5),unreal.Rotator(yaw=-90),False,True)
text=a.get_component_by_class(unreal.TextRenderComponent);text.modify();text.set_text('RELAY OVERLOOK\nSENSOR GALLERY 03');text.set_world_size(14);text.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER);text.set_hidden_in_game(False,False)
text.set_vertical_alignment(unreal.VerticalTextAligment.EVRTA_TEXT_CENTER)
sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem);lib=unreal.SubobjectDataBlueprintFunctionLibrary
handles=sub.k2_gather_subobject_data_for_instance(a);parent=next(h for h in handles if lib.get_associated_object(lib.get_data(h))==a)
h,reason=sub.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=parent,new_class=unreal.StaticMeshComponent,conform_transform_to_parent=True));assert lib.is_handle_valid(h),reason
sub.rename_subobject(h,'DestinationPlaque');plate=lib.get_associated_object(lib.get_data(h));plate.set_static_mesh(meshes['SM_Aurelion_KIT_Z03DestinationPlaque']);plate.set_world_transform(unreal.Transform(location=unreal.Vector(7600,-15045,210),rotation=unreal.Rotator(yaw=180),scale=unreal.Vector(1,1,1)),False,True);plate.set_collision_profile_name('NoCollision');plate.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
assert helper['snapshot_actor_state'](untouched)==before
settings=runpy.run_path(str(root/'Scripts/Editor/check_z03_wayfinding.py'))['check_z03_wayfinding'](actors)
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'z03-wayfinding-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',settings=settings),indent=2))
capture=(root/'Scripts/Editor/review_eclipse_wall_scars.py').read_text();prefix=capture.split('views=[',1)[0];suffix=capture.split('state=dict',1)[1]
views="views=[('approach',(6800,-19300,165),(0,90),90),('middle',(6800,-17200,165),(5,90),90),('destination',(7440,-15800,210),(0,78),70),('register',(7240,-18900,120),(-55,90),65)]\n"
exec(compile(prefix+views+'state=dict'+suffix,'z03_wayfinding_review','exec'),globals())
