from pathlib import Path
import unreal
SAVE_GATE_ASSEMBLY=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z06_gate_lantern.py').read_text(),'save_gate_assembly','exec'),globals())
