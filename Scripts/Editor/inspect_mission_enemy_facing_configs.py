"""Read-only inventory of placed combat NPC facing and fire-presentation settings."""
import json
import os
from collections import defaultdict
from pathlib import Path

import unreal


out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY']) / 'enemy-facing-configs.json'
groups = defaultdict(lambda: dict(count=0, names=[], hard_lock=None,
                                  turn_rate=None, fire_montage=None))
for actor in unreal.EditorLevelLibrary.get_all_level_actors():
    if not isinstance(actor, unreal.SovNPCCharacterBase):
        continue
    key = actor.get_class().get_path_name()
    row = groups[key]
    row['count'] += 1
    row['names'].append(actor.get_name())
    row['hard_lock'] = actor.get_editor_property('permits_hard_lock')
    row['turn_rate'] = actor.get_editor_property('combat_facing_turn_rate')
    montage = actor.get_editor_property('weapon_fire_montage')
    row['fire_montage'] = montage.get_path_name() if montage else None
out.write_text(json.dumps(dict(status='passed', groups=groups), indent=2), encoding='utf-8')
