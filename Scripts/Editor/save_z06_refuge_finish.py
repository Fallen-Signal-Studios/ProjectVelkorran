from pathlib import Path
import unreal
PERSIST=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z06_refuge_finish.py').read_text(),'preview_z06_refuge_finish','exec'),globals())
