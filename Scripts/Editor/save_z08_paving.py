from pathlib import Path
import unreal
PERSIST=True;SKIP_PAVING_CAPTURE=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z08_paving.py').read_text(),'save_z08_paving','exec'),globals())
