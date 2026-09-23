"""Replace the grenade's stale sample-pack weapon metadata with canonical items.

The grenade is universal through its native gate override. This keeps its saved
Blueprint metadata aligned with the weapon contexts that actually grant it.
"""
import json
import os
from pathlib import Path

import unreal


out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY']) / 'grenade-weapon-metadata.json'
path = '/Game/Abilities/Tarrik/GA_Tarrik_CinderStickyGrenade'
bp = unreal.load_asset(path)
if not bp:
    raise RuntimeError('Missing grenade ability')
cdo = unreal.get_default_object(bp.generated_class())
before = [cls.get_path_name() for cls in cdo.get_editor_property('allowed_weapon_classes')]
wanted = [unreal.load_class(None, '/Game/Items/Weapons/WI_' + name + '.WI_' + name + '_C')
          for name in ('Velkorran', 'Cinderline')]
if any(cls is None for cls in wanted):
    raise RuntimeError('Missing canonical Tarrik weapon class')
cdo.set_editor_property('allowed_weapon_classes', wanted)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
if not unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False):
    raise RuntimeError('Grenade ability did not save')
after = [cls.get_path_name() for cls in unreal.get_default_object(bp.generated_class()).get_editor_property('allowed_weapon_classes')]
if after != [cls.get_path_name() for cls in wanted]:
    raise RuntimeError('Grenade weapon metadata did not persist')
out.write_text(json.dumps({'status': 'saved', 'asset': path, 'before': before, 'after': after}, indent=2), encoding='utf-8')
unreal.log('TARRIK_GRENADE_WEAPON_METADATA saved')
