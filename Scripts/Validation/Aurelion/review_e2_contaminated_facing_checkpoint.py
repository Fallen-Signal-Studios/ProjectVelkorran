"""Read-only contaminated-drone observer alongside an earned E2 checkpoint replay."""
import runpy
from pathlib import Path


here = Path(__file__).resolve().parent
runpy.run_path(str(here / 'observe_e2_contaminated_drone_facing.py'))
runpy.run_path(str(here / 'review_e2_contaminated_checkpoint.py'))
