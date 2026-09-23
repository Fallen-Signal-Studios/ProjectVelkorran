"""Fresh normal-input E1 with passive enemy-decision and native-death observers."""
import runpy
from pathlib import Path

here = Path(__file__).resolve().parent
runpy.run_path(str(here / 'observe_e1_enemy_pressure.py'))
runpy.run_path(str(here / 'validate_aurelion_entry_pie.py'))
