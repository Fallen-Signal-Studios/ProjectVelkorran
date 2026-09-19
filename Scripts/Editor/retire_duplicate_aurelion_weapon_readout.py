"""Collapse only the legacy vitals/weapon container now covered by the holographic HUD."""
import json
import os
import shutil
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
bp = unreal.load_asset('/Game/Aurelion/UI/WBP_AurelionGameplayHUD')
author = unreal.SovWidgetTreeAuthoringLibrary
container = author.find_widget_in_tree(bp, 'VerticalBox_0')
assert isinstance(container, unreal.VerticalBox)
assert {w.get_name() for w in container.get_all_children()} == {'WBP_WeaponInfo', 'WBP_PlayerInfo_HUD'}
assert author.find_widget_in_tree(bp, 'WBP_PlayerInfo_HUD').get_visibility() == unreal.SlateVisibility.COLLAPSED
source = Path(unreal.Paths.project_dir()) / 'Content/Aurelion/UI/WBP_AurelionGameplayHUD.uasset'
backup = out / 'WBP_AurelionGameplayHUD.before-retire-readout.uasset'
assert not backup.exists()
shutil.copy2(source, backup)
tree = list(author.describe_widget_tree(bp))
before = str(container.get_visibility())
container.set_visibility(unreal.SlateVisibility.COLLAPSED)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert author.find_widget_in_tree(bp, 'VerticalBox_0').get_visibility() == unreal.SlateVisibility.COLLAPSED
assert list(author.describe_widget_tree(bp)) == tree
assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
(out / 'legacy-readout-retired.json').write_text(json.dumps(dict(
    status='saved_requires_runtime_review', asset=bp.get_path_name(), before=before,
    after='Collapsed', children=['WBP_WeaponInfo', 'WBP_PlayerInfo_HUD'], tree_preserved=True), indent=2))
