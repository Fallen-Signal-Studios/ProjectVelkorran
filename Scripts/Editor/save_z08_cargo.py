from pathlib import Path
import unreal
PERSIST_Z08_CARGO=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z08_cargo.py').read_text(),'save_cargo','exec'),globals())
