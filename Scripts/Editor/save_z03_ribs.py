from pathlib import Path
import unreal
PERSIST_Z03_RIBS=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z03_ribs.py').read_text(),'save_z03_ribs','exec'),globals())
