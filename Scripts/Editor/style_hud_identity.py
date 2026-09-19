"""Style identity text after wrapping PlateBars in an Overlay in UMG. No graph rebuild."""
import unreal, os, json
from pathlib import Path
bp=unreal.load_asset('/Game/Aurelion/UI/HUD/WBP_SovHolographicHUD')
a=unreal.SovWidgetTreeAuthoringLibrary
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
bars=a.find_widget_in_tree(bp,'PlateBars')
assert isinstance(bars.get_parent(),unreal.Overlay)
name=a.find_widget_in_tree(bp,'ProtagonistName')
if not name:
    a.add_widget_to_tree(bp,unreal.TextBlock,'ProtagonistName',bars.get_parent())
# Structural changes reinstance the tree; never reuse old widget references.
bars=a.find_widget_in_tree(bp,'PlateBars')
name=a.find_widget_in_tree(bp,'ProtagonistName')
assert name and bars
layers=bars.get_parent()
layers.slot.set_padding(unreal.Margin(0,0,0,0))
bars.slot.set_padding(unreal.Margin(90,16,56,13))
bars.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
bars.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
name.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
name.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_TOP)
name.slot.set_padding(unreal.Margin(0,-2,0,0))
name.set_text(unreal.Text(''))
font=name.get_editor_property('font')
font.set_editor_property('size',12)
font.set_editor_property('letter_spacing',1400)
name.set_editor_property('font',font)
name.set_editor_property('justification',unreal.TextJustify.CENTER)
name.set_color_and_opacity(unreal.SlateColor(unreal.LinearColor(1,1,1,1)))
name.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
bindings=list(a.describe_widget_bindings(bp))
assert len(bindings)==10 and all('typeMatches=1' in r for r in bindings)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
Path(os.environ['SOV_AURELION_RUN_DIRECTORY'],'identity-layout.json').write_text(json.dumps({'status':'layout_saved','bindings':bindings,'note':'Text and palette bindings are authored in UMG; verify them in PIE.'},indent=2))
