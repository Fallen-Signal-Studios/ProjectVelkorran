"""Persist the reviewed Z06 custom wall cassette and capture the saved level."""
from pathlib import Path
import runpy
import unreal

root=Path(unreal.Paths.project_dir())
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
already=any(a.get_actor_label()=='KIT_Z06_Wayfinding_BreachRescue'
            for a in actors.get_all_level_actors())
if already:
    runpy.run_path(str(root/'Scripts/Editor/check_z06_wayfinding_panel.py'))
else:
    runpy.run_path(str(root/'Scripts/Editor/preview_z06_wayfinding_panel.py'),
                   init_globals={'PERSIST_Z06_WAYFINDING':True})
