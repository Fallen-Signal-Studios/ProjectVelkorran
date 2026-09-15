from pathlib import Path
import unreal
PERSIST_CARRIER_KIT=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_carrier_kit.py').read_text(),'save_carrier_kit','exec'),globals())
