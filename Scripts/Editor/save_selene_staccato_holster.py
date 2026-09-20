"""Save only the reviewed project Staccato BackB holster configuration."""
import runpy
from pathlib import Path
runpy.run_path(str(Path(__file__).with_name('refine_selene_staccato_holster.py')),
              init_globals={'SAVE_HOLSTER':True},run_name='__main__')
