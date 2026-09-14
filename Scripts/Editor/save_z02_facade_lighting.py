from pathlib import Path
import unreal
PERSIST=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z02_facade_lighting.py').read_text(encoding='utf-8-sig'),'preview_z02_facade_lighting','exec'))
