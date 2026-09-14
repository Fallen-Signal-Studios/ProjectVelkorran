"""Unsaved explicit-tangent diagnostic at the original stone normal strength."""
from pathlib import Path
import time
import unreal
import json,os
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
mesh=unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Meshes/SM_Aurelion_KIT_Z06ClimbPanel')
sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);settings=sm.get_nanite_settings(mesh)
assert not settings.get_editor_property('explicit_tangents')
settings.set_editor_property('explicit_tangents',True);sm.set_nanite_settings(mesh,settings,True)
assert sm.get_nanite_settings(mesh).get_editor_property('explicit_tangents')
(out/'explicit-tangent-preview.json').write_text(json.dumps(dict(saved=False,mesh=mesh.get_path_name(),explicit_tangents=True)))
unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
def review_tick(delta):
    if time.monotonic()-started<15:return
    unreal.unregister_slate_post_tick_callback(review_handle)
    try:
        pass
        global editor,subsystem,by_label
        editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);by_label={a.get_actor_label():a for a in subsystem.get_all_level_actors()}
        editor.editor_set_game_view(True)
        capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
        capture=capture.replace("('entry',unreal.Vector(p.x,p.y,p.z+165),unreal.Rotator(yaw=90),90)","('e3-explicit-front',unreal.Vector(-900,9370,-440),unreal.Rotator(pitch=0,yaw=170),85)")
        capture=capture.replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('e3-explicit-angle',unreal.Vector(-1000,9050,-440),unreal.Rotator(pitch=0,yaw=140),85)").replace('z01-','wall-')
        exec(compile("p=by_label['Z08_Entry_StandIn']"+capture,'wall_contact_review','exec'),globals())
    except Exception:
        unreal.EditorPythonScripting.set_keep_python_script_alive(False);raise
review_handle=unreal.register_slate_post_tick_callback(review_tick)
