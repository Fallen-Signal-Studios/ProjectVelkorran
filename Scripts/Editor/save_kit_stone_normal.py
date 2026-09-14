from pathlib import Path
import unreal
SAVE_STONE_NORMAL=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_kit_stone_normal.py').read_text(),'preview_kit_stone_normal','exec'),globals())
