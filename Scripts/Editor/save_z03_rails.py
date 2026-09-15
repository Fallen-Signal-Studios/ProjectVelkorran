from pathlib import Path
import unreal
PERSIST_Z03_RAILS=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z03_rails.py').read_text(),'save_z03_rails','exec'),globals())
