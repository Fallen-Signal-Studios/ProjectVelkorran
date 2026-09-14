from pathlib import Path
import unreal
PERSIST=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z06_lighting.py').read_text(),'save_z06_lighting','exec'))
