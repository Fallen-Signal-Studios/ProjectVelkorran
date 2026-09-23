"""Observe actual Enforcer weapon attack timestamps in an earned E2 retry."""
import runpy
from pathlib import Path


here = Path(__file__).resolve().parent
runpy.run_path(str(here / 'observe_e2_enforcer_weapon_fire.py'))
runpy.run_path(str(here / 'review_e2_contaminated_checkpoint.py'))
