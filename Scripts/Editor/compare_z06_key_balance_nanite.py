"""Unsaved Nanite detail isolation at normal shadow settings and neutral local keys."""
from pathlib import Path
import json,os
import unreal
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
name='r.Nanite.MaxPixelsPerEdge';before=unreal.SystemLibrary.get_console_variable_float_value(name)
unreal.SystemLibrary.execute_console_command(world,name+' 0.25');assert abs(unreal.SystemLibrary.get_console_variable_float_value(name)-.25)<.0001
(Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])/'nanite-detail-isolation.json').write_text(json.dumps(dict(variable=name,before=before,preview=.25,saved=False,scope='Image comparison only; GPU cost not qualified.'),indent=2))
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z06_key_balance.py').read_text(),'neutral_key_nanite_isolation','exec'),globals())
