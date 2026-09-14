from pathlib import Path
import unreal
SAVE_CLIMB_PANEL=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z06_climb.py').read_text(),'save_climb','exec'),globals())
