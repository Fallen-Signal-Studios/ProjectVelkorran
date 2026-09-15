from pathlib import Path
import unreal
PERSIST_Z09_PLINTHS=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z09_plinths.py').read_text(),'save_z09_plinths','exec'),globals())
