"""Compare increased pixel coverage by approach and doubled capture resolution."""
from pathlib import Path
import unreal
SURFACE_VIEWS=['isolated-close','isolated-2x']
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/diagnose_z09_surface_isolation.py').read_text(),'z09_surface_coverage','exec'),globals())
