"""Capture the normal gameplay camera on a native companion health-damage receipt.

Fresh route and ordinary weapon input only. Screenshot existence is not visual
acceptance; inspect whether the actual Eclipse blood is visible at the real hit.
"""
from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).with_name('check_fresh_tarrik_focus_contact.py')),
              init_globals={'PLAYER_CONTRIBUTION_PROBE':True,'BLOOD_VISUAL_PROBE':True,
                            'CONTACT_SCOPE':__doc__})
