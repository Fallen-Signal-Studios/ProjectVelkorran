from pathlib import Path
import runpy
import unreal
root=Path(unreal.Paths.project_dir())
exec(compile((root/'Scripts/Editor/verify_z01_endwalls.py').read_text(encoding='utf-8-sig'),'verify_z01_endwalls','exec'))
geometry=runpy.run_path(str(root/'Scripts/Editor/check_z01_cover.py'))['check_cover'](world,actors)
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out/'cover-reload-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),geometry=geometry),indent=2))
