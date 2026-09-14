"""Save the reviewed Z01 lower-wall fit, with original map backup and invariants."""
from pathlib import Path
import unreal
source=Path(unreal.Paths.project_dir())/'Scripts/Editor/fit_z01_lower_architecture.py'
exec(compile(source.read_text(encoding='utf-8-sig'),str(source),'exec'),{'PERSIST':True,'__file__':str(source)})
