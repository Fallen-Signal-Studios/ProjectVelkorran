from pathlib import Path
import unreal
PERSIST=True;SKIP_COURT_CAPTURE=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z08_court.py').read_text(),'save_z08_court','exec'),globals())
