"""Fresh-load gallery and preceding architecture verification."""
from pathlib import Path
import runpy
import unreal
root=Path(unreal.Paths.project_dir())
exec(compile((root/'Scripts/Editor/verify_z02_perimeter.py').read_text(encoding='utf-8-sig'),'verify_z02_perimeter','exec'))
assert len(actors)==2005+z02_furniture_count+z02_guard_count+z02_bridges_count+z02_exterior_count and z02_gallery_count==6
geometry=runpy.run_path(str(root/'Scripts/Editor/check_z02_gallery.py'))['check_gallery'](world,actors)
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out/'z02-gallery-reload-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),geometry=geometry),indent=2))
