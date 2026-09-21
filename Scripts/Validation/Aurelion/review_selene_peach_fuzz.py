"""Compare the unchanged CP9 face with only its peach-fuzz groom temporarily hidden."""
from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).with_name('review_selene_face_streaming.py')),
              init_globals={'FACE_GROOM_ISOLATION':True})
