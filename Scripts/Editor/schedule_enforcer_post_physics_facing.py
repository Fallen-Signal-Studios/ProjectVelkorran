"""Order the Enforcer combat turn after movement and mesh updates."""
import json
import os
from pathlib import Path

import unreal


bp = unreal.load_asset('/Game/Aurelion/Enemies/BP_AurelionEnforcer')
assert bp
cdo = unreal.get_default_object(bp.generated_class())
assert cdo.get_editor_property('permits_hard_lock')
before = cdo.get_editor_property('primary_actor_tick').export_text()
tick = cdo.get_editor_property('primary_actor_tick')
group_type = type(tick.get_editor_property('tick_group'))
tick.set_editor_property('tick_group', group_type.TG_POST_PHYSICS)
cdo.set_editor_property('primary_actor_tick', tick)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
after = unreal.get_default_object(bp.generated_class()).get_editor_property(
    'primary_actor_tick').export_text()
assert 'TickGroup=TG_PostPhysics' in after, after
Path(os.environ['SOV_AURELION_RUN_DIRECTORY'], 'enforcer-post-physics.json').write_text(
    json.dumps(dict(status='passed', asset=bp.get_path_name(), before=before,
                    after=after), indent=2), encoding='utf-8')
