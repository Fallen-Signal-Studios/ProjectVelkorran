"""Fit existing custom wayfinding modules and retire obsolete relay decorations."""
from pathlib import Path
import json,os,runpy,shutil,unreal
root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/Z04WayfindingKit';out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());labels={a.get_actor_label():a for a in actors}
old=json.loads((source/'guidance-baseline.json').read_text());fit=json.loads((source/'presentation-baseline.json').read_text())
check=runpy.run_path(str(root/'Scripts/Editor/check_z04_wayfinding.py'))
helper=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
changed={r['actor'] for r in fit['decorations']}|{old['actor'],'Aurelion_Art_Sign_Z04_0e7c69'}
untouched=[a for a in actors if a.get_actor_label() not in changed];before=helper['snapshot_actor_state'](untouched)
for row in fit['decorations']:
    c=labels[row['actor']].get_component_by_class(unreal.StaticMeshComponent)
    assert c.get_world_transform().export_text()==row['transform'] and str(c.get_collision_enabled())==row['collision']
    c.modify();c.set_hidden_in_game(True,False);c.set_collision_profile_name('NoCollision');c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
for row in fit['components']:
    if row['class_name']=='TextRenderComponent' and row['actor'].startswith('Z04_'):
        c=labels[row['actor']].get_component_by_class(unreal.TextRenderComponent);assert str(c.text)==row['properties']['text']
        c.modify();c.set_hidden_in_game(True,False)
dest='/Game/Aurelion/Environment/ArchitectureKit/Meshes/'
c=labels[old['actor']].get_component_by_class(unreal.InstancedStaticMeshComponent)
assert c.static_mesh.get_path_name()==old['mesh'] and [c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]==old['all_instance_transforms']
c.modify();c.clear_instances();c.set_static_mesh(unreal.load_asset(dest+'SM_Aurelion_KIT_Z03RouteRegister'));c.set_editor_property('override_materials',[]);c.set_collision_profile_name('NoCollision');c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
for p,yaw in check['route_placements']():c.add_instance(unreal.Transform(location=unreal.Vector(*p),rotation=unreal.Rotator(yaw=yaw),scale=unreal.Vector(1,1,1)),world_space=True)
a=labels['Aurelion_Art_Sign_Z04_0e7c69'];a.modify();a.set_actor_location_and_rotation(unreal.Vector(6400,-9131.4,242.5),unreal.Rotator(yaw=-90),False,True)
text=a.get_component_by_class(unreal.TextRenderComponent);text.modify();text.set_text('MEETING ATRIUM\nRELAY OVERLOOK 04');text.set_world_size(14);text.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER);text.set_vertical_alignment(unreal.VerticalTextAligment.EVRTA_TEXT_CENTER);text.set_hidden_in_game(False,False)
sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem);lib=unreal.SubobjectDataBlueprintFunctionLibrary
handles=sub.k2_gather_subobject_data_for_instance(a);parent=next(h for h in handles if lib.get_associated_object(lib.get_data(h))==a)
h,reason=sub.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=parent,new_class=unreal.StaticMeshComponent,conform_transform_to_parent=True));assert lib.is_handle_valid(h),reason
sub.rename_subobject(h,'DestinationPlaque');plate=lib.get_associated_object(lib.get_data(h));plate.set_static_mesh(unreal.load_asset(dest+'SM_Aurelion_KIT_Z03DestinationPlaque'));plate.set_world_transform(unreal.Transform(location=unreal.Vector(6400,-9125.4,210),rotation=unreal.Rotator(yaw=180),scale=unreal.Vector(1,1,1)),False,True);plate.set_collision_profile_name('NoCollision');plate.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
assert helper['snapshot_actor_state'](untouched)==before
settings=check['check_z04_wayfinding'](actors)
persist=bool(globals().get('PERSIST_Z04_WAYFINDING',False))
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'z04-wayfinding-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',settings=settings),indent=2))
capture=(root/'Scripts/Editor/review_eclipse_wall_scars.py').read_text();prefix=capture.split('views=[',1)[0];suffix=capture.split('state=dict',1)[1]
views="views=[('approach',(7100,-12700,165),(0,90),90),('route',(7200,-12000,165),(-15,90),85),('destination',(6800,-9800,210),(0,120),70),('balcony',(9300,-10600,465),(5,135),85)]\n"
exec(compile(prefix+views+'state=dict'+suffix,'z04_wayfinding_review','exec'),globals())
