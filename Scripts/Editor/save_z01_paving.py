"""Save reviewed paving and retire the two raised decorative strip colliders."""
from pathlib import Path
import unreal
source=Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z01_paving.py'
exec(compile(source.read_text(encoding='utf-8-sig'),str(source),'exec'),{'PERSIST':True,'__file__':str(source)})
