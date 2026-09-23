"""Let the close-range Linkbound track a target that crosses its attack arc."""
import json
import os
from pathlib import Path

import unreal


asset = unreal.load_asset('/Game/Aurelion/Enemies/BP_AurelionLinkbound')
assert asset
cdo = unreal.get_default_object(asset.generated_class())
before = dict(
    permits_hard_lock=cdo.get_editor_property('permits_hard_lock'),
    tick=cdo.get_editor_property('primary_actor_tick').export_text(),
    turn_rate=cdo.get_editor_property('combat_facing_turn_rate'),
)
assert before['permits_hard_lock']
assert 'TickGroup=TG_PostPhysics' in before['tick']
assert abs(before['turn_rate'] - 360.0) < 0.01
cdo.set_editor_property('combat_facing_turn_rate', 720.0)
assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
after = unreal.get_default_object(asset.generated_class()).get_editor_property(
    'combat_facing_turn_rate')
assert abs(after - 720.0) < 0.01
Path(os.environ['SOV_AURELION_RUN_DIRECTORY'], 'linkbound-turn-rate.json').write_text(
    json.dumps(dict(status='passed', asset=asset.get_path_name(), before=before,
                    after_turn_rate=after), indent=2), encoding='utf-8')
