"""Save the reviewed measured bridge replacement, backing up M12 first."""
from pathlib import Path
import unreal
source=Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z01_bridge.py'
exec(compile(source.read_text(encoding='utf-8-sig'),str(source),'exec'),{'PERSIST':True,'__file__':str(source)})
