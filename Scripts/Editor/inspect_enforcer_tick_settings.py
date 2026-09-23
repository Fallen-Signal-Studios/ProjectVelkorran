"""Read-only inspection of the Enforcer's combat-facing and actor tick defaults."""
import json
import os
from pathlib import Path

import unreal


bp = unreal.load_asset('/Game/Aurelion/Enemies/BP_AurelionEnforcer')
assert bp
cdo = unreal.get_default_object(bp.generated_class())
row = dict(class_path=bp.generated_class().get_path_name(),
           permits_hard_lock=cdo.get_editor_property('permits_hard_lock'),
           turn_rate=cdo.get_editor_property('combat_facing_turn_rate'))
for key in ('primary_actor_tick', 'actor_tick_group', 'tick_group'):
    try:
        value = cdo.get_editor_property(key)
        row[key] = value.export_text() if hasattr(value, 'export_text') else str(value)
    except Exception as error:
        row[key] = {'unavailable': str(error)}
Path(os.environ['SOV_AURELION_RUN_DIRECTORY'], 'enforcer-tick-settings.json').write_text(
    json.dumps(row, indent=2), encoding='utf-8')
