from pathlib import Path
import unreal
SAVE_FLANK_LANDING=True
SKIP_LANDING_CAPTURE=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z06_flank_landing.py').read_text(),'save_flank_landing','exec'),globals())
