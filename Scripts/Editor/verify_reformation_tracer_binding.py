"""Fresh saved-reader verification with controlled simulation and render."""
from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).with_name('inspect_reformation_tracer.py')))
runpy.run_path(str(Path(__file__).with_name('preview_reformation_tracer_binding.py')),
              init_globals={'EXPECTED_READER':'Tracer'})
