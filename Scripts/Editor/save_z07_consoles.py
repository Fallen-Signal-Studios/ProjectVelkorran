from pathlib import Path
import unreal
PERSIST=True;SKIP_CONSOLE_CAPTURE=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z07_consoles.py').read_text(),'save_z07_consoles','exec'),globals())
