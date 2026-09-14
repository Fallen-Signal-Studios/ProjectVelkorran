from pathlib import Path
import unreal
PERSIST=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_atrium_bridge_floors.py').read_text(),'save_atrium_bridge_floors','exec'))
