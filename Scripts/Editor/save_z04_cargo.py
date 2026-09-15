from pathlib import Path
import unreal
PERSIST_Z04_CARGO=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z04_cargo.py').read_text(),'save_z04_cargo','exec'),globals())
