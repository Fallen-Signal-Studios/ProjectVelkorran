"""Fresh editor and PIE readback of the additive ray renderer; no map saves."""
from pathlib import Path
import hashlib,json,os,time,traceback,unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor()
maps=[root/'Content/Aurelion/Maps'/n for n in ('L_Aurelion_M12.umap','L_Aurelion_M13.umap')]
hashes={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in maps}
report=dict(status='running')
def check(actors):
    stars=[a for a in actors if a.get_class().get_name()=='BP_Star1_C'];assert len(stars)==1
    star=stars[0]
    components={c.get_name():c for c in star.get_components_by_class(unreal.StaticMeshComponent)}
    assert components['SM_Rays'].get_editor_property('disallow_nanite')
    assert not components['SM_Star'].get_editor_property('disallow_nanite')
    return dict(actor=star.get_path_name(),components={name:dict(
        disallow_nanite=components[name].get_editor_property('disallow_nanite'),
        mesh=components[name].static_mesh.get_path_name(),
        materials=[m.get_path_name() if m else None for m in components[name].get_materials()])
        for name in ('SM_Rays','SM_Star')})
report['editor']=check(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
state=dict(start=time.monotonic(),stopping=False)
def tick(dt):
    try:
        if state['stopping']:
            if not level.is_in_play_in_editor():
                unreal.unregister_slate_post_tick_callback(handle)
                unreal.EditorPythonScripting.set_keep_python_script_alive(False)
            return
        assert time.monotonic()-state['start']<90,'PIE readback timed out'
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if not world or time.monotonic()-state['start']<20:return
        report['pie']=check(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor))
        report['maps_unchanged']=all(hashlib.sha256(p.read_bytes()).hexdigest()==hashes[str(p)] for p in maps)
        assert report['maps_unchanged'];report['status']='passed'
    except Exception:report.update(status='failed',error=traceback.format_exc())
    else:
        if report['status']=='running':return
    (out/'star-rays-readback.json').write_text(json.dumps(report,indent=2))
    level.editor_request_end_play();state['stopping']=True
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle=unreal.register_slate_post_tick_callback(tick)
level.editor_request_begin_play()
