"""Observe Enforcer facing during an earned E2 checkpoint retry in PIE."""
import runpy
from pathlib import Path


here = Path(__file__).resolve().parent
runpy.run_path(str(here / 'observe_e2_enforcer_facing.py'))
runpy.run_path(str(here / 'review_e2_contaminated_checkpoint.py'))
