"""Fresh-process verification of the saved material pass; no gameplay/visual claim."""
import json
import os
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
bp = unreal.load_asset('/Game/Aurelion/UI/HUD/WBP_SovHolographicHUD')
author = unreal.SovWidgetTreeAuthoringLibrary
bindings = list(author.describe_widget_bindings(bp))
assert len(bindings) == 10 and all('typeMatches=1' in row for row in bindings)
materials = {}
for widget_name, material_name in [('ArcFill', 'M_SovEchoSegmentedArc'), ('RadarDisc', 'M_SovRadarReticle'), ('PlateRegion', 'M_SovPlateHousing'), ('AmmoRegion', 'M_SovAmmoHousing')]:
    widget = author.find_widget_in_tree(bp, widget_name)
    material = widget.get_editor_property('background' if widget_name.endswith('Region') else 'brush').get_editor_property('resource_object')
    expected = '/Game/Aurelion/UI/HUD/' + material_name
    assert material.get_path_name() == expected + '.' + material_name
    assert material.get_editor_property('material_domain') == unreal.MaterialDomain.MD_UI
    assert material.get_editor_property('blend_mode') == unreal.BlendMode.BLEND_TRANSLUCENT
    materials[widget_name] = material.get_path_name()
assert author.find_widget_in_tree(bp, 'EchoBar').get_visibility() == unreal.SlateVisibility.COLLAPSED
health = author.find_widget_in_tree(bp, 'HealthBar')
assert health.get_editor_property('bar_fill_style') == unreal.ProgressBarFillStyle.MASK
for field in ('fill_image', 'background_image'):
    resource = health.get_editor_property('widget_style').get_editor_property(field).get_editor_property('resource_object')
    assert resource.get_path_name() == '/Game/Aurelion/UI/HUD/M_SovHealthBeveledFill.M_SovHealthBeveledFill'
    materials['HealthBar.'+field] = resource.get_path_name()
gameplay_hud = unreal.load_asset('/Game/Aurelion/UI/WBP_AurelionGameplayHUD')
compass = author.find_widget_in_tree(gameplay_hud, 'WBP_Navigator_Compass')
assert isinstance(compass.slot, unreal.CanvasPanelSlot)
assert compass.slot.get_offsets().top == 148.0
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out / 'hud-material-verification.json').write_text(json.dumps({
    'status': 'passed', 'bindings': bindings, 'materials': materials,
    'qualification': 'Fresh saved-asset references and native widget bindings only. PIE rendering, palette/event bindings, pips, contacts and accessibility remain unqualified.'
}, indent=2), encoding='utf-8')
unreal.log('HUD_MATERIAL_SAVED_VERIFICATION_PASSED')
