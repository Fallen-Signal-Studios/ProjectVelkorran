"""Read-only fixed-camera baseline for the Z11 ceiling review."""
from pathlib import Path
import hashlib
import json
import os
import runpy
import unreal

root = Path(unreal.Paths.project_dir())
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name() == 'L_Aurelion_M13' and not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
paths = {n:root/'Content/Aurelion/Maps'/n for n in ('L_Aurelion_M12.umap','L_Aurelion_M13.umap')}
before = {n:hashlib.sha256(p.read_bytes()).hexdigest() for n,p in paths.items()}
runpy.run_path(str(root/'Scripts/Editor/preview_m13_route.py'),init_globals={
    'M13_ROUTE_VIEWS':[('z11-ceiling-entry',(650,42650,190)),
                       ('z11-ceiling-room',(700,43100,190)),
                       ('z11-ceiling-under',(0,43100,190))],
    'M13_ROUTE_YAWS':{'z11-ceiling-entry':170,'z11-ceiling-room':180,'z11-ceiling-under':90},
    'M13_ROUTE_PITCHES':{'z11-ceiling-entry':19,'z11-ceiling-room':23,'z11-ceiling-under':42},
})
assert {n:hashlib.sha256(p.read_bytes()).hexdigest() for n,p in paths.items()} == before
(out/'z11-ceiling-before.json').write_text(json.dumps(dict(status='read_only',map_sha256=before),indent=2))
print('Z11_CEILING_BEFORE_READ_ONLY_PASS')
