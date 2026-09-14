from pathlib import Path
import unreal
PERSIST=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_atrium_rings.py').read_text(),'save_atrium_rings','exec'))
