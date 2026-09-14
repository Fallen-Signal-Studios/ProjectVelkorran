"""Verify saved climb shading and inspect both authored climb faces."""
from pathlib import Path
import time
import unreal
root=Path(unreal.Paths.project_dir());DEFER_Z08_LABELS_AUTORUN=True
exec(compile((root/'Scripts/Editor/verify_z08_technical_labels.py').read_text(),'verify_wall_contact_bindings','exec'),globals())
del DEFER_Z08_LABELS_AUTORUN
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
        capture=capture.replace("('entry',unreal.Vector(p.x,p.y,p.z+165),unreal.Rotator(yaw=90),90)","('blue-shapes-baseline',unreal.Vector(-2450,19350,-920),unreal.Rotator(pitch=15,yaw=160),75)")
        capture=capture.replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('without-retry-holograms',unreal.Vector(-2450,19350,-920),unreal.Rotator(pitch=15,yaw=160),75)").replace('z01-','wall-')
        global holograms
        holograms=[c for a in subsystem.get_all_level_actors() for c in a.get_components_by_class(unreal.StaticMeshComponent) if a.get_actor_label() in ('Aurelion_E4A_RetryEncounter','Aurelion_E4B_RetryEncounter') and c.static_mesh and c.static_mesh.get_name()=='SM_KB3D_OAS_PropHologramWall_A']
        assert len(holograms)==2 and all(c.get_editor_property('visible') for c in holograms)
        capture=capture.replace("if phase%2==0:","if phase%2==0:\n                for c in holograms:c.set_visibility(name!='without-retry-holograms')")
        capture=capture.replace("elif phase==4 and elapsed>60:","elif phase==4 and elapsed>60:\n            for c in holograms:c.set_visibility(True)")
        exec(compile("p=by_label['Z08_Entry_StandIn']"+capture,'blue_shape_isolation','exec'),globals())
    except Exception:
        unreal.EditorPythonScripting.set_keep_python_script_alive(False);raise
review_handle=unreal.register_slate_post_tick_callback(review_tick)
