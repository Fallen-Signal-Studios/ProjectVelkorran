"""Play fresh E1→E2 with ordinary input while observing Enforcer rifle shots."""
import os
import runpy
from pathlib import Path


here = Path(__file__).resolve().parent
os.environ['SOV_AURELION_ROUTE_STOP_AFTER'] = 'continue_aurelion_e2_input'
runpy.run_path(str(here / 'observe_e2_enforcer_weapon_fire.py'))
runpy.run_path(str(here / 'observe_e2_enforcer_facing.py'))
runpy.run_path(str(here / 'start_e1_death_pressure_route.py'))
