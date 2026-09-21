"""Read the full departure wall batch and native room geometry, without rendering."""
from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).with_name('audit_m13_departure_props.py')),
    init_globals={'SKIP_DEPARTURE_CAPTURE':True})
