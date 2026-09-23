"""Attach a read-only WallRunner pose observer to the normal E4B replay."""
import runpy
from pathlib import Path


here = Path(__file__).resolve().parent
runpy.run_path(str(here / 'observe_e4b_wall_runner.py'))
runpy.run_path(str(here / 'review_e4b_checkpoint_route.py'))
