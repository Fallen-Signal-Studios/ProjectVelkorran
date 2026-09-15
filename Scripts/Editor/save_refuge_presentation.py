from pathlib import Path
import unreal
PERSIST_REFUGE_PRESENTATION=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_refuge_presentation.py').read_text(),'save_refuge_presentation','exec'),globals())
