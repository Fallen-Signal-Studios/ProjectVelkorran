from pathlib import Path
import unreal
PERSIST_Z04_WAYFINDING=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z04_wayfinding.py').read_text(),'save_z04_wayfinding','exec'),globals())
