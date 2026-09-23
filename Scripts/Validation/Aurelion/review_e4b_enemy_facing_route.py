"""Observe attack facing alongside the unmodified normal-input E4B checkpoint replay."""
import runpy
from pathlib import Path

here = Path(__file__).resolve().parent
runpy.run_path(str(here / 'observe_e4b_enemy_facing.py'))
runpy.run_path(str(here / 'review_e4b_checkpoint_route.py'))
