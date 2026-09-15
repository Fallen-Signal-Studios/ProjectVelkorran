from pathlib import Path
import unreal
PERSIST_REFUGE_SHELL=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_refuge_shell.py').read_text(),'save_refuge_shell','exec'),globals())
