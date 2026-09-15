from pathlib import Path
import unreal
PERSIST_Z04_RAILS=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z04_rails.py').read_text(),'save_z04_rails','exec'),globals())
