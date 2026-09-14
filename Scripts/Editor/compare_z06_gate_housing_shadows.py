"""Unsaved direct-shadow isolation with the saved stone materials and same cameras."""
from pathlib import Path
import json,os
import unreal
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
name='r.ShadowQuality';before=unreal.SystemLibrary.get_console_variable_int_value(name)
unreal.SystemLibrary.execute_console_command(world,name+' 0')
assert unreal.SystemLibrary.get_console_variable_int_value(name)==0
(Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])/'shadow-isolation-settings.json').write_text(json.dumps(dict(variable=name,before=before,preview=0,saved=False),indent=2))
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z06_gate_housing.py').read_text(),'preview_z06_gate_housing','exec'),globals())
