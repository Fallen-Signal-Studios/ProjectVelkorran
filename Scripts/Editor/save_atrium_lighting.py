from pathlib import Path
import unreal
PERSIST=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_atrium_lighting.py').read_text(),'save_atrium_lighting','exec'))
