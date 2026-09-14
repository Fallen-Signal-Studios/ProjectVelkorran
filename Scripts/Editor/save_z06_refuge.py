from pathlib import Path
import unreal
PERSIST=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z06_refuge.py').read_text(),'preview_z06_refuge','exec'),globals())
