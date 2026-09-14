"""Same housing cameras and Nanite setting with only stone overridden in memory."""
from pathlib import Path
import unreal
PLAIN_HOUSING_STONE=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z06_gate_housing.py').read_text(),'preview_z06_gate_housing','exec'),globals())
