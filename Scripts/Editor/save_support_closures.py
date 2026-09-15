from pathlib import Path
import unreal
PERSIST_SUPPORT_CLOSURES=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_support_closures.py').read_text(),'save_support_closures','exec'),globals())
