"""Save the reviewed Z01 vault fit with a pre-save map backup."""
from pathlib import Path
import unreal
source=Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z01_vault.py'
exec(compile(source.read_text(encoding='utf-8-sig'),str(source),'exec'),{'PERSIST':True,'__file__':str(source)})
