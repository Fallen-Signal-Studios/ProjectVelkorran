"""Same housing cameras with mesh-local Nanite disabled in memory, without saving."""
from pathlib import Path
import unreal
DISABLE_HOUSING_NANITE=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z06_gate_housing.py').read_text(),'preview_z06_gate_housing','exec'),globals())
