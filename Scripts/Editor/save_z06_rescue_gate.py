from pathlib import Path
import unreal
PERSIST=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z06_rescue_gate.py').read_text(),'preview_z06_rescue_gate','exec'),globals())
