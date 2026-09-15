from pathlib import Path
import unreal
PERSIST_Z03_WAYFINDING=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z03_wayfinding.py').read_text(),'save_z03_wayfinding','exec'),globals())
