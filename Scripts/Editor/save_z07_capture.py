from pathlib import Path
import unreal
PERSIST=True;SKIP_CAPTURE_VIEWS=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z07_capture.py').read_text(),'save_z07_capture','exec'),globals())
