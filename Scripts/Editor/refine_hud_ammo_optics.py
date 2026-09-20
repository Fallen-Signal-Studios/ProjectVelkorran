"""Refine the existing ammo panel without replacing its native live text binding."""
import json
import os
import shutil
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
root = '/Game/Aurelion/UI/HUD/'
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
edit = unreal.MaterialEditingLibrary
material = unreal.load_asset(root + 'M_SovAmmoHousing')
bp = unreal.load_asset(root + 'WBP_SovHolographicHUD')
author = unreal.SovWidgetTreeAuthoringLibrary
bindings = list(author.describe_widget_bindings(bp))
assert len(bindings) == 10 and all('typeMatches=1' in b for b in bindings)
visited, shapes = set(), []

def visit(node):
    if not node or node.get_path_name() in visited:
        return
    visited.add(node.get_path_name())
    if isinstance(node, unreal.MaterialExpressionCustom):
        shapes.append(node)
    for upstream in edit.get_inputs_for_material_expression(material, node):
        visit(upstream)

visit(edit.get_material_property_input_node(material, unreal.MaterialProperty.MP_EMISSIVE_COLOR))
assert len(shapes) == 1
shape = shapes[0]
before = shape.get_editor_property('code')
assert 'Three cartridges:' in before and 'float outerHalo=' in before
for name in ('M_SovAmmoHousing', 'WBP_SovHolographicHUD'):
    source = Path(unreal.Paths.project_dir()) / 'Content/Aurelion/UI/HUD' / (name + '.uasset')
    target = out / (name + '.before.uasset')
    assert not target.exists()
    shutil.copy2(source, target)
(out / 'ammo-before.hlsl').write_text(before)

code = r'''
float aa=max(fwidth(UV.y),.002);
float aspect=max(fwidth(UV.y)/max(fwidth(UV.x),.00001),1.);
// Chamfer the left corners and retain the reference's sloping right edge.
float left=.04+max(abs(UV.y-.48)-.27,0)/aspect;
float right=.84+UV.y*.13;
float d=max(max(left-UV.x,UV.x-right)*aspect,max(.06-UV.y,UV.y-.90));
float body=1-smoothstep(0,aa,d);
float rim=(1-smoothstep(.006,.006+aa,abs(d+.020)))*body;
float innerBevel=exp(-abs(d+.055)*75)*body;
// Three cartridges: pointed tips, separate casing and base band.
float cartridge=0;
for(int i=0;i<3;i++) {
    float2 b=UV-float2(.125+i*.042,.49);
    float width=.012*saturate((b.y+.23)/.11);
    float bulletD=max(abs(b.x)-width,max(-.23-b.y,b.y-.20));
    float shape=1-smoothstep(0,max(fwidth(UV.x),.001),bulletD);
    float seam=1-smoothstep(.008,.014,abs(b.y-.075));
    cartridge=max(cartridge,shape*(1-seam*.70));
}
float upperGlint=exp(-abs(UV.y-.104)*95)*body;
float outerHalo=exp(-max(d,0)*52)*(1-body);
float brightness=body*.003+rim*.88+innerBevel*.11+upperGlint*.09+outerHalo*.14+cartridge*.97;
// More translucent glass, with independently opaque luminous trim and glyphs.
return float4(brightness.xxx,saturate(body*.82+rim*.18+cartridge+outerHalo*.25));
'''
shape.set_editor_property('code', code)
edit.recompile_material(material)
text = author.find_widget_in_tree(bp, 'AmmoText')
font = text.get_editor_property('font')
old_font = str(font)
font.set_editor_property('size', 26)
text.set_editor_property('font', font)
text.get_editor_property('slot').set_editor_property('vertical_alignment', unreal.VerticalAlignment.V_ALIGN_CENTER)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert list(author.describe_widget_bindings(bp)) == bindings
for asset in (material, bp):
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
(out / 'ammo-after.hlsl').write_text(code)
(out / 'ammo-optics.json').write_text(json.dumps(dict(
    status='saved_requires_gameplay_visual_review', old_font=old_font,
    font_size=26, native_bindings=bindings, layout_rectangles_changed=False), indent=2))
