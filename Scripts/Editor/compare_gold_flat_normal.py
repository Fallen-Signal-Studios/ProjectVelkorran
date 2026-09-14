from pathlib import Path
import unreal
COMPARE_GOLD_FLAT_NORMAL=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_basalt_fine_normal.py').read_text(),'compare_gold_flat_normal','exec'),globals())
