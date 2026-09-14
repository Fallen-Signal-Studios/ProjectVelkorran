"""Unsaved gate housing fit and native sweep preview before map integration."""
import json,os,runpy,time
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);original=list(subsystem.get_all_level_actors());by_label={a.get_actor_label():a for a in original};assert len(original)==2837
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));before=helpers['snapshot_actor_state'](original);source=root/'Art/Source/Aurelion/Z06GateHousingKit'
destination='/Game/Aurelion/Environment/ArchitectureKit';materials={key:destination+'/Materials/M_AurelionKit_'+value for key,value in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_StoneGrout':'StoneGrout','M_Aurelion_AncientGold':'Gold'}.items()}
unreal.SystemLibrary.execute_console_command(world,'Interchange.FeatureFlags.Import.FBX 0')
mesh=helpers['import_owned_mesh'](json.loads((source/'manifest.json').read_text())['modules'][0],source,destination+'/Meshes',materials)
if globals().get('DISABLE_HOUSING_NANITE',False):
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);settings=sm.get_nanite_settings(mesh);settings.set_editor_property('enabled',False);sm.set_nanite_settings(mesh,settings,True)
door=by_label['Aurelion_E3_RescueAccess'];a=subsystem.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(1100,8460,-600),door.get_actor_rotation());a.set_actor_label('KIT_Z06_GateHousing');a.set_folder_path('Aurelion/CustomArchitecture/Z06/Refuge');c=a.static_mesh_component;c.set_static_mesh(mesh);c.set_collision_profile_name('BlockAll');c.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS);a.set_actor_enable_collision(True)
if globals().get('PLAIN_HOUSING_STONE',False):
    plain=unreal.load_asset('/Engine/BasicShapes/BasicShapeMaterial');assert plain
    for i,slot in enumerate(mesh.get_editor_property('static_materials')):
        if str(slot.get_editor_property('imported_material_slot_name'))=='M_Aurelion_IvoryStone':c.set_material(i,plain)
assert helpers['snapshot_actor_state'](original)==before
o,e=a.get_actor_bounds(False);assert abs(o.z+e.z-25)<.02 and abs(o.z-e.z+600)<.02
geometry=runpy.run_path(str(root/'Scripts/Editor/check_z06_rescue_gate.py'))['check_z06_rescue_gate'](world,list(subsystem.get_all_level_actors()))
# Isolated architectural clearance through the opening: this excludes the closed moving leaf.
lanes=[];ignored=[other for other in original]
for x in (925,1100,1275):
    for z in (-505,-420):
        raw=unreal.SystemLibrary.capsule_trace_single_by_profile(world,unreal.Vector(x,8200,z),unreal.Vector(x,8700,z),42,88,'Pawn',False,ignored,unreal.DrawDebugTrace.NONE,True)
        h=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
        assert not (h and h.to_tuple()[0]),(x,z,h);lanes.append([x,z])
(out/'gate-housing-preview.json').write_text(json.dumps(dict(status='unsaved_preview',housing_nanite_enabled=not globals().get('DISABLE_HOUSING_NANITE',False),plain_stone=bool(globals().get('PLAIN_HOUSING_STONE',False)),gate=geometry,housing_top_cm=25,aperture_capsules=lanes,qualification='Native door sweeps and isolated housing aperture only; map not saved, live interaction and final art pending.'),indent=2))
editor.editor_set_game_view(True)
capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
capture=capture.replace("('entry',unreal.Vector(p.x,p.y,p.z+165),unreal.Rotator(yaw=90),90)","('housing-front',unreal.Vector(1100,7550,-330),unreal.Rotator(pitch=6,yaw=90),80)").replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('housing-rear',unreal.Vector(400,9600,-120),unreal.Rotator(pitch=-5,yaw=-60),85)").replace('z01-','z06-')
exec(compile("p=by_label['Z06_Entry_StandIn']"+capture,'gate_housing_capture','exec'))
