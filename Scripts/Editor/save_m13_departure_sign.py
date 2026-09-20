"""Save the reviewed faction-arrow correction to M13 and its owned text table."""
from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).with_name('correct_m13_departure_sign.py')),
              init_globals={'SAVE_DEPARTURE_SIGN':True})
