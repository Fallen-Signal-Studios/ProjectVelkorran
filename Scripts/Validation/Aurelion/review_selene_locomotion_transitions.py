"""CP2 jump/landing and direction reversal through existing ability/movement input APIs.

Not physical keyboard/controller coverage. No teleport, grants or asset mutation.
"""
from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).with_name('review_selene_posture_play.py')),init_globals={
    'POSTURE_CONTINUOUS_REVERSAL':True,
    'POSTURE_REVIEW_CASES':('restored','jump','landed','walk','walk_reverse','stop')})
