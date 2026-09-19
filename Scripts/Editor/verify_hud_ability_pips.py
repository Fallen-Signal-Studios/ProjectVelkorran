"""Read saved pip cells and existing HUD bindings in a fresh editor process."""
import runpy
from pathlib import Path
import os, json, unreal
runpy.run_path(str(Path(unreal.Paths.project_dir())/'Scripts/Editor/verify_holographic_hud_material_pass.py'))
bp=unreal.load_asset('/Game/Aurelion/UI/HUD/WBP_SovHolographicHUD')
author=unreal.SovWidgetTreeAuthoringLibrary
pips=author.find_widget_in_tree(bp,'AbilityPips')
assert isinstance(pips,unreal.Image)
assert isinstance(pips.slot,unreal.OverlaySlot)
assert pips.get_parent()==author.find_widget_in_tree(bp,'ProtagonistName').get_parent()
mat=pips.get_editor_property('brush').get_editor_property('resource_object')
assert mat.get_path_name()=='/Game/Aurelion/UI/HUD/M_SovAbilityPips.M_SovAbilityPips'
assert mat.get_editor_property('material_domain')==unreal.MaterialDomain.MD_UI
assert mat.get_editor_property('blend_mode')==unreal.BlendMode.BLEND_TRANSLUCENT
assert pips.get_visibility()==unreal.SlateVisibility.HIT_TEST_INVISIBLE
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
(Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])/'hud-pips-saved.json').write_text(json.dumps(dict(status='passed',material=mat.get_path_name(),scope='Saved widget and material references; runtime state coverage is separate'),indent=2))
