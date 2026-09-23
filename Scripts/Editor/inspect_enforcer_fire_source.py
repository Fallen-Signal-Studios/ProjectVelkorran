"""Read-only source, mesh and current Enforcer attack-presentation inspection."""
import json
import os
from pathlib import Path

import unreal


out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
source = unreal.load_asset('/Game/Sci_Fi_Characters_Pack/AnimDemoScene/Animations/Fire_Rifle_Ironsights')
appearance = unreal.load_asset('/Game/Aurelion/Art/Characters/Appearances/CA_AurelionEnforcer')
bp = unreal.load_asset('/Game/Aurelion/Enemies/BP_AurelionEnforcer')
reload_montage = unreal.load_asset('/NarrativePro/Pro/Core/Character/Biped/Animation/Sequences/Weapon/Rifle/3P/AM_Rifle_3P_Reload')
assert source and appearance and bp and reload_montage
mesh = appearance.get_editor_property('character_attributes').get_editor_property('base_mesh')
cdo = unreal.get_default_object(bp.generated_class())
report = dict(source=source.get_path_name(), source_seconds=source.get_play_length(),
    source_skeleton=source.get_editor_property('skeleton').get_path_name(),
    source_notifies=len(unreal.AnimationLibrary.get_animation_notify_events(source)),
    source_root_motion=source.get_editor_property('enable_root_motion'),
    mesh=mesh.get_path_name(), mesh_skeleton=mesh.get_editor_property('skeleton').get_path_name(),
    enforcer_class=cdo.get_class().get_path_name(),
    reload_skeleton=reload_montage.get_editor_property('skeleton').get_path_name(),
    map_dirty=[str(item) for item in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()])
(out / 'enforcer-fire-source.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
