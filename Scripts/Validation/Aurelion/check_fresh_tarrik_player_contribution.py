"""Fresh route, one ordinary player shot at the Elite, then Tarrik/Linkbound contact.

The native damage receipt must confirm a nonlethal player hit. No damage values,
grants, positions, health, contribution budgets or encounter state are supplied.
"""
from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).with_name('check_fresh_tarrik_focus_contact.py')),
              init_globals={'PLAYER_CONTRIBUTION_PROBE':True,'CONTACT_SCOPE':__doc__})
