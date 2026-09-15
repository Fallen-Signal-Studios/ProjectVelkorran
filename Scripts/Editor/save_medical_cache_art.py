from pathlib import Path
import unreal
PERSIST_MEDICAL_CACHE=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_medical_cache_art.py').read_text(),'save_medical_cache','exec'),globals())
