from pathlib import Path
import unreal
PERSIST=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_atrium_guards.py').read_text(),'preview_atrium_guards','exec'))
