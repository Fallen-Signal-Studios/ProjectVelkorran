"""Preview finite-size chamber wall lights, retaining intensity and shadow casting."""
from pathlib import Path
import json,os,runpy,unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Aurelion_M13' and not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
lights=[a for a in actors.get_all_level_actors() if a.get_actor_label().startswith('ENVL_M13_ChamberUplight_')]
assert len(lights)==8
rows=[]
for a in lights:
    c=a.get_component_by_class(unreal.SpotLightComponent);assert c
    before=dict(source_radius=c.get_editor_property('source_radius'),intensity=c.intensity,cast_shadows=c.get_editor_property('cast_shadows'))
    c.modify();c.set_source_radius(30)
    rows.append(dict(actor=a.get_actor_label(),before=before,source_radius_cm=c.get_editor_property('source_radius')))
(out/'soft-uplight-preview.json').write_text(json.dumps(dict(saved=False,lights=rows),indent=2))
runpy.run_path(str(Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_m13_route.py'),init_globals=dict(
    ALLOW_DIRTY_PREVIEW=True,M13_ROUTE_VIEWS=[('chamber',(0,33600,-1570)),('wall-detail',(3400,37000,-1300))],M13_ROUTE_YAWS={'wall-detail':30}))
