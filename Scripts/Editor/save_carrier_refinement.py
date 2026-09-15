from pathlib import Path
import unreal
PERSIST_CARRIER_REFINEMENT=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_carrier_refinement.py').read_text(),'save_carrier_refinement','exec'),globals())
