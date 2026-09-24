"""Apply reviewed conversation visuals and the tablet glass Nanite correction."""
from pathlib import Path
import runpy

root = Path(__file__).resolve().parents[2]
runpy.run_path(str(root/'Scripts/Editor/fix_z11_witness_tablet_nanite.py'))
runpy.run_path(str(root/'Scripts/Editor/refine_m13_z11_observation_furniture.py'))
