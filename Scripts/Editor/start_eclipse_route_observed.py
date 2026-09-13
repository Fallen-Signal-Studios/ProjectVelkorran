"""Start an unchanged fresh route with a separate read-only Eclipse pose census."""
from pathlib import Path
import runpy
import unreal
root=Path(unreal.Paths.project_dir()).resolve()
runpy.run_path(str(root/'Scripts/Editor/observe_eclipse_route.py'))
runpy.run_path(str(root/'Scripts/Validation/Aurelion/validate_aurelion_entry_pie.py'))
