from pathlib import Path
import unreal
PERSIST=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_north_bridges.py').read_text(),'preview_north_bridges','exec'))
