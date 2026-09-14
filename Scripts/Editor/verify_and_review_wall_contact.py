"""Check saved contact bindings, then inspect both authored climb faces."""
from pathlib import Path
import time
import unreal
root=Path(unreal.Paths.project_dir());DEFER_WALL_CONTACT_AUTORUN=True
exec(compile((root/'Scripts/Editor/verify_wall_contact_bindings.py').read_text(),'verify_wall_contact_bindings','exec'),globals())
del DEFER_WALL_CONTACT_AUTORUN
unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
def review_tick(delta):
    if time.monotonic()-started<15:return
    unreal.unregister_slate_post_tick_callback(review_handle)
    try:
        verify()
        global editor,subsystem,by_label
        editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);by_label={a.get_actor_label():a for a in subsystem.get_all_level_actors()}
        editor.editor_set_game_view(True)
        capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
        capture=capture.replace("('entry',unreal.Vector(p.x,p.y,p.z+165),unreal.Rotator(yaw=90),90)","('e4-contact-face',unreal.Vector(1770,21830,-1020),unreal.Rotator(pitch=0,yaw=165),80)")
        capture=capture.replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('e3-contact-face',unreal.Vector(-900,9370,-440),unreal.Rotator(pitch=0,yaw=170),85)").replace('z01-','wall-')
        exec(compile("p=by_label['Z08_Entry_StandIn']"+capture,'wall_contact_review','exec'),globals())
    except Exception:
        unreal.EditorPythonScripting.set_keep_python_script_alive(False);raise
review_handle=unreal.register_slate_post_tick_callback(review_tick)
