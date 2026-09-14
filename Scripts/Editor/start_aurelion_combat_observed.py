"""Fresh unchanged route with read-only native damage and Eclipse observers."""
from pathlib import Path
import runpy
import unreal
root = Path(unreal.Paths.project_dir()).resolve()
runpy.run_path(str(root/'Scripts/Editor/observe_aurelion_combat.py'))
runpy.run_path(str(root/'Scripts/Editor/observe_eclipse_damage.py'))
runpy.run_path(str(root/'Scripts/Editor/start_eclipse_route_observed.py'))
