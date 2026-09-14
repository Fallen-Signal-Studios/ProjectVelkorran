from pathlib import Path
import unreal
SAVE_COVER_COFFERS=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z06_cover_coffers.py').read_text(),'save_cover_coffers','exec'),globals())
