from pathlib import Path
import unreal
PERSIST_Z08_LABELS=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z08_technical_labels.py').read_text(),'save_z08_technical_labels','exec'),globals())
