"""Measure the current rescue gate's visual, attachment and full moving-box sweep."""
import json,os,runpy
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());labels={a.get_actor_label():a for a in actors};assert len(actors)==2837
door=labels['Aurelion_E3_RescueAccess'];body=door.moving_body;visual=door.visual
def xyz(v):return [v.x,v.y,v.z]
def component(c):
    return dict(name=c.get_name(),parent=c.get_attach_parent().get_name() if c.get_attach_parent() else None,world_transform=c.get_world_transform().export_text(),relative_transform=c.get_relative_transform().export_text(),collision=str(c.get_collision_enabled()),profile=str(c.get_collision_profile_name()),mesh=c.static_mesh.get_path_name() if isinstance(c,unreal.StaticMeshComponent) and c.static_mesh else None)
hit=runpy.run_path(str(root/'Scripts/Editor/validate_aurelion_transit_clearance.py'))['_hit']
start=body.get_world_location();end=unreal.MathLibrary.transform_location(door.scene_root.get_world_transform(),door.destination_offset);extent=body.get_scaled_box_extent();sweeps=[]
for name,a,b in [('initial',start,start+unreal.Vector(0,0,.1)),('open',start,end),('close',end,start)]:
    result=hit(unreal.SystemLibrary.box_trace_single_by_profile(world,a,b,extent,body.get_world_rotation(),body.get_collision_profile_name(),False,[door],unreal.DrawDebugTrace.NONE,True));sweeps.append(dict(name=name,result=result))
data=dict(status='read_only',actor=door.get_actor_label(),actor_transform=door.get_actor_transform().export_text(),transit_id=str(door.transit_id),destination_offset=xyz(door.destination_offset),travel_seconds=door.travel_seconds,required_mission=str(door.required_mission),required_beat=str(door.required_beat),body_extent=xyz(extent),components=[component(c) for c in door.get_components_by_class(unreal.PrimitiveComponent)],sweeps=sweeps,qualification='Current stopped-editor baseline; no runtime transit or wave-release qualification')
(out/'rescue-gate-baseline.json').write_text(json.dumps(data,indent=2))
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
print('Z06_RESCUE_GATE_SURVEY_PASS')
