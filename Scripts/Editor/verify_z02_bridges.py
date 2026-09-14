from pathlib import Path
import runpy
import unreal
root=Path(unreal.Paths.project_dir())
exec(compile((root/'Scripts/Editor/verify_z02_approach_guards.py').read_text(encoding='utf-8-sig'),'verify_z02_approach_guards','exec'))
assert len(actors)==2049+z02_exterior_count and z02_bridges_count==4
checks=runpy.run_path(str(root/'Scripts/Editor/check_z02_bridges.py'))
geometry=checks['check_bridges'](world,actors)
geometry['route_controls']=checks['route_controls'](world,actors)
for row in geometry['route_controls']:
    assert row['blocker']==('Aurelion_PressureHallExit' if row['bridge']=='bridge2' else None),row
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out/'z02-bridges-reload-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),geometry=geometry),indent=2))
