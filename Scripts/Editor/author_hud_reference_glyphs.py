"""Add reference glyphs to existing UI materials, preserving graph and bindings."""
import json
import os
import shutil
from pathlib import Path
import unreal

OUT = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
ROOT = '/Game/Aurelion/UI/HUD/'
AUTHOR = unreal.SovWidgetTreeAuthoringLibrary
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()

def backup(name):
    source = Path(unreal.Paths.project_dir())/'Content/Aurelion/UI/HUD'/(name+'.uasset')
    target = OUT/(name+'.before.uasset')
    if not target.exists():
        shutil.copy2(source, target)

def update(name, code):
    backup(name)
    material = unreal.load_asset(ROOT+name)
    owned = []
    for expression in unreal.ObjectIterator(unreal.MaterialExpressionCustom):
        outer = expression.get_outer()
        while outer:
            if outer == material:
                owned.append(expression)
                break
            outer = outer.get_outer()
    assert len(owned) == 1, 'Inspect unexpected custom expression count: '+name
    owned[0].set_editor_property('code', code)
    unreal.MaterialEditingLibrary.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)

update('M_SovPlateHousing', r'''
float2 p = UV;
float aa = max(fwidth(p.y),.002);
float halfW = .49-abs(p.y-.59)*.08;
float dist = max(abs(p.x-.5)-halfW,abs(p.y-.59)-.235);
float body = 1-smoothstep(0,aa,dist);
float rim = (1-smoothstep(.008,.008+aa,abs(dist+.021)))*body;
float cap = step(abs(p.x-.5),.22)*step(.015,p.y)*step(p.y,.16);
float shelf = step(abs(p.x-.5),.24)*step(.86,p.y)*step(p.y,.98);
float shield = step(abs(p.x-.5),.44)*step(.19,p.y)*step(p.y,.31);
// Reference medical cross. Correct the region's wide aspect before shaping.
float2 h = (p-float2(.068,.565))*float2(8.333333,1);
float crossD = min(max(abs(h.x)-.036,abs(h.y)-.125),max(abs(h.x)-.125,abs(h.y)-.036));
float medical = 1-smoothstep(0,aa,crossD);
// Small filled escutcheon in the upper shield channel.
float2 s = (p-float2(.068,.25))*float2(8.333333,1);
float sw = .065*saturate((.092-s.y)/.065);
float sd = max(abs(s.x)-sw,max(-.065-s.y,s.y-.092));
float shieldGlyph = 1-smoothstep(0,aa,sd);
float glint = (1-smoothstep(.006,.006+aa,abs(h.y+.116)))*medical;
float glyph = max(medical,shieldGlyph);
float brightness = .015*(body+cap+shelf+shield)+rim*.65+glyph*.95+glint*.18;
return float4(brightness.xxx,saturate((body+cap+shelf+shield)*.83+glyph));
''')

update('M_SovAmmoHousing', r'''
float aa=max(fwidth(UV.y),.002);
float left=.04+max(.24-UV.y,0)*.14;
float right=.84+UV.y*.13;
float d=max(max(left-UV.x,UV.x-right),max(.06-UV.y,UV.y-.90));
float body=1-smoothstep(0,aa,d);
float rim=(1-smoothstep(.009,.009+aa,abs(d+.026)))*body;
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
return float4((body*.018+rim*.62+cartridge*.95).xxx,saturate(body*.86+cartridge));
''')

backup('WBP_SovHolographicHUD')
bp = unreal.load_asset(ROOT+'WBP_SovHolographicHUD')
for name in ('PlateRegion', 'AmmoRegion'):
    border = AUTHOR.find_widget_in_tree(bp, name)
    brush = border.get_editor_property('background')
    brush.set_editor_property('tint_color', unreal.SlateColor(unreal.LinearColor(1,1,1,1)))
    border.set_editor_property('background', brush)
AUTHOR.find_widget_in_tree(bp,'PlateBars').slot.set_editor_property('padding',unreal.Margin(90,16,56,13))
AUTHOR.find_widget_in_tree(bp,'AmmoText').slot.set_editor_property('padding',unreal.Margin(62,8,18,8))
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
bindings = list(AUTHOR.describe_widget_bindings(bp))
assert len(bindings)==10 and all('typeMatches=1' in row for row in bindings)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(OUT/'reference-glyphs.json').write_text(json.dumps({'status':'saved_pending_visual_review','bindings':bindings},indent=2))
unreal.log('HUD_REFERENCE_GLYPHS_SAVED')
