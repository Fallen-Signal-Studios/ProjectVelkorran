from pathlib import Path
import unreal
root = Path(unreal.Paths.project_dir())
exec(compile((root/'Scripts/Editor/enable_cargo_chaos_materials.py').read_text(), 'enable_cargo_chaos_materials', 'exec'), globals())
exec(compile((root/'Scripts/Editor/validate_destructible_cover_runtime.py').read_text(), 'validate_destructible_cover_runtime', 'exec'), globals())
