from pathlib import Path
import unreal
PERSIST=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_atrium_parapets.py').read_text(),'save_atrium_parapets','exec'))
