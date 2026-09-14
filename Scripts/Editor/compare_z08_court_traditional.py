from pathlib import Path
import unreal
COMPARE_TRADITIONAL=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z08_court.py').read_text(),'compare_z08_court_traditional','exec'),globals())
