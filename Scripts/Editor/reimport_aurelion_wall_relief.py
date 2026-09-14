"""Reimport the two shared wall variants with separated base and reveal caps."""
from pathlib import Path
import json,os,runpy,shutil,time
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);source=root/'Art/Source/Aurelion/ArchitectureKit'
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);original=list(subsystem.get_all_level_actors());by_label={a.get_actor_label():a for a in original};assert len(original)==2843
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));before=helpers['snapshot_actor_state'](original)
assert not json.loads((root/'Art/Source/Aurelion/Z06SurfaceReview/wall-coplanar-faces.json').read_text())['overlaps']
specs={r['asset']:r for r in json.loads((source/'manifest.json').read_text())['modules']}
roundtrip={r['asset']:r for r in json.loads((source/'verification.json').read_text())['modules']}
destination='/Game/Aurelion/Environment/ArchitectureKit/Meshes';rows=[]
unreal.SystemLibrary.execute_console_command(world,'Interchange.FeatureFlags.Import.FBX 0')
for name in ('SM_Aurelion_KIT_WallPlain_4x7','SM_Aurelion_KIT_WallBay_4x7'):
    old=unreal.load_asset(destination+'/'+name);assert old
    bounds=old.get_bounds();origin=unreal.Vector(bounds.origin.x,bounds.origin.y,bounds.origin.z);extent=unreal.Vector(bounds.box_extent.x,bounds.box_extent.y,bounds.box_extent.z)
    materials={str(slot.get_editor_property('imported_material_slot_name')):slot.get_editor_property('material_interface').get_path_name() for slot in old.get_editor_property('static_materials')}
    spec=dict(specs[name]);spec['nominal_dimensions_m']=roundtrip[name]['dimensions_m']
    mesh=helpers['import_owned_mesh'](spec,source,destination,materials);b=mesh.get_bounds()
    assert (b.origin-origin).length()<.01 and (b.box_extent-extent).length()<.01
    rows.append(dict(mesh=name,materials=materials,dimensions_m=spec['nominal_dimensions_m']))
assert helpers['snapshot_actor_state'](original)==before
geometry=runpy.run_path(str(root/'Scripts/Editor/check_z06_walls.py'))['check_z06_walls'](world,original)
map_saved=False
if unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages():
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level();map_saved=True
(out/'wall-relief-reimport.json').write_text(json.dumps(dict(status='meshes_saved',map_saved=map_saved,meshes=rows,geometry=geometry,scope='Shared base/reveal cap separation; original material assignments and mesh envelopes retained.'),indent=2))
editor.editor_set_game_view(True)
capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
capture=capture.replace("('entry',unreal.Vector(p.x,p.y,p.z+165),unreal.Rotator(yaw=90),90)","('climb-face',unreal.Vector(-850,9150,-340),unreal.Rotator(pitch=-10,yaw=150),65)").replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('housing-front',unreal.Vector(1100,7550,-330),unreal.Rotator(pitch=6,yaw=90),80)").replace('z01-','z06-')
exec(compile("p=by_label['Z06_Entry_StandIn']"+capture,'wall_relief_capture','exec'),globals())
