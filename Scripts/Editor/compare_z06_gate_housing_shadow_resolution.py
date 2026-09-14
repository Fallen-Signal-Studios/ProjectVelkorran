"""Unsaved VSM resolution comparison at the same housing cameras, with soft shadows on."""
from pathlib import Path
import json,os
import unreal
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
name='r.Shadow.Virtual.ResolutionLodBiasLocal'
before=unreal.SystemLibrary.get_console_variable_float_value(name)
unreal.SystemLibrary.execute_console_command(world,name+' -2')
assert unreal.SystemLibrary.get_console_variable_float_value(name)==-2
(Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])/'shadow-resolution-settings.json').write_text(json.dumps(dict(variable=name,before=before,preview=-2,saved=False),indent=2))
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z06_gate_housing.py').read_text(),'preview_z06_gate_housing','exec'),globals())
