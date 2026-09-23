"""Observe real WallRunner traversal during an ordinary-input fresh route."""
import runpy
from pathlib import Path


here = Path(__file__).resolve().parent
runpy.run_path(str(here / 'observe_e4b_wall_runner.py'))
runpy.run_path(str(here / 'start_e1_death_pressure_route.py'))
