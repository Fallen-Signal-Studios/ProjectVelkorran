from pathlib import Path
import unreal
Z03_WIDE_FILL=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z03_ceiling_light.py').read_text(),'z03_wide_fill','exec'),globals())
