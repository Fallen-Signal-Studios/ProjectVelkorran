from pathlib import Path
import unreal
PERSIST=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z02_gallery.py').read_text(encoding='utf-8-sig'),'preview_z02_gallery','exec'))
