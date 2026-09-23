"""Observe Tarrik's ordinary companion combat during an earned E2 retry."""
import os
import runpy
import sys
from pathlib import Path


here = Path(__file__).resolve().parent
sys.path.insert(0, str(here))
import observe_companion_animation
import observe_companion_weapon_hit_path


observe_companion_animation.start('e2-companion-animation.json')
observe_companion_weapon_hit_path.start('e2-companion-hit-path.json')
os.environ['SOV_E2_PASSIVE_SECONDS'] = '60'
runpy.run_path(str(here / 'review_e2_contaminated_checkpoint.py'))
