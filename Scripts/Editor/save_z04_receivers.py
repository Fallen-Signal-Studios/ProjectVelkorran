from pathlib import Path
import unreal
PERSIST_Z04_RECEIVERS=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z04_receivers.py').read_text(),'save_z04_receivers','exec'),globals())
