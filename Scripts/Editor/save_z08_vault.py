from pathlib import Path
import unreal
PERSIST=True;SKIP_VAULT_CAPTURE=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z08_vault.py').read_text(),'save_z08_vault','exec'),globals())
