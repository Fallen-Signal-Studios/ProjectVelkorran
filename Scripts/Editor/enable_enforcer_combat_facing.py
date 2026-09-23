"""Enable the shared observed-target combat turn for the M12 rifle Enforcer."""
import json
import os
from pathlib import Path

import unreal


bp = unreal.load_asset('/Game/Aurelion/Enemies/BP_AurelionEnforcer')
assert bp
cdo = unreal.get_default_object(bp.generated_class())
before = bool(cdo.get_editor_property('permits_hard_lock'))
if not before:
    cdo.set_editor_property('permits_hard_lock', True)
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
after = bool(unreal.get_default_object(bp.generated_class()).get_editor_property('permits_hard_lock'))
assert after
Path(os.environ['SOV_AURELION_RUN_DIRECTORY'], 'enforcer-facing-change.json').write_text(
    json.dumps(dict(status='passed', asset=bp.get_path_name(), before=before,
                    after=after, saved=not before), indent=2), encoding='utf-8')
