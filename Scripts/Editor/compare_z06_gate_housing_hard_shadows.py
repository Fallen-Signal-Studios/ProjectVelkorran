"""Unsaved local-light hard-shadow comparison, preserving geometry and light output."""
from pathlib import Path
import json,os
import unreal
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
name='r.Shadow.Virtual.SMRT.RayCountLocal'
before=unreal.SystemLibrary.get_console_variable_int_value(name)
unreal.SystemLibrary.execute_console_command(world,name+' 0')
assert unreal.SystemLibrary.get_console_variable_int_value(name)==0
(Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])/'hard-shadow-settings.json').write_text(json.dumps(dict(variable=name,before=before,preview=0,saved=False),indent=2))
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z06_gate_housing.py').read_text(),'preview_z06_gate_housing','exec'),globals())
