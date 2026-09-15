from pathlib import Path
import unreal
PERSIST_Z04_PIERS = True
exec(compile((Path(unreal.Paths.project_dir()) / 'Scripts/Editor/preview_z04_piers.py').read_text(), 'save_z04_piers', 'exec'), globals())
