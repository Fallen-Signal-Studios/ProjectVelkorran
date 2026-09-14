from pathlib import Path
import unreal
KEY_BALANCE_WIDE_CAPTURE=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z06_key_balance.py').read_text(),'wide_key_balance','exec'),globals())
