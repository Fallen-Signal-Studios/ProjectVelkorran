from pathlib import Path
import unreal
SAVE_KEY_BALANCE=True
SKIP_KEY_CAPTURE=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z06_key_balance.py').read_text(),'save_key_balance','exec'),globals())
