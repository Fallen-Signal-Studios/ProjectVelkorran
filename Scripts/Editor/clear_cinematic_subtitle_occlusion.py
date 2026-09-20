"""Retire decorative black bars drawn over native subtitles in the owned HUD.

Only WBP_AurelionGameplayHUD is saved. The nested overlay keeps its skip/pause
bindings and animation lifecycle; its two decorative images no longer obscure
the native accessibility surface. Camera aspect ratios and sequences are untouched.
"""
import hashlib
import json
import os
import shutil
from pathlib import Path
import unreal

assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
root = Path(unreal.Paths.project_dir()).resolve()
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY']) / 'CinematicSubtitleRepair'
out.mkdir(exist_ok=False)
map_path = root / 'Content/Aurelion/Maps/L_Aurelion_M12.umap'
map_hash = hashlib.sha256(map_path.read_bytes()).hexdigest()
asset = root / 'Content/Aurelion/UI/WBP_AurelionGameplayHUD.uasset'
shutil.copy2(asset, out / asset.name)
bp = unreal.load_asset('/Game/Aurelion/UI/WBP_AurelionGameplayHUD')
author = unreal.SovWidgetTreeAuthoringLibrary
overlay = author.find_widget_in_tree(bp, 'WBP_CinematicOverlay')
assert overlay.get_class().get_path_name() == '/NarrativePro/Pro/Core/UI/Menus/Cinematic/WBP_CinematicOverlay.WBP_CinematicOverlay_C'
before = overlay.get_render_opacity()
overlay.set_render_opacity(0.)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
overlay = author.find_widget_in_tree(bp, 'WBP_CinematicOverlay')
assert overlay.get_render_opacity() == 0.
assert hashlib.sha256(map_path.read_bytes()).hexdigest() == map_hash
(out / 'repair.json').write_text(json.dumps(dict(status='saved_requires_fresh_visual_review',
    before_opacity=before, after_opacity=overlay.get_render_opacity(), protected_m12_hash=map_hash), indent=2))
