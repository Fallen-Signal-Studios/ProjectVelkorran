"""Save the bounded presentation refinement, then run the real fresh entry route."""
from pathlib import Path
import runpy
import unreal
root=Path(unreal.Paths.project_dir()).resolve()
runpy.run_path(str(root/'Scripts/Editor/refine_eclipse_readability.py'))
runpy.run_path(str(root/'Scripts/Validation/Aurelion/validate_aurelion_entry_pie.py'))
