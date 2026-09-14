from pathlib import Path
import unreal
SAVE_REQUEST_PROPS=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z06_request_props.py').read_text(),'save_request_props','exec'),globals())
