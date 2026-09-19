"""Refine reference silhouettes inside the existing native HUD layout rectangles."""
import json
import os
import shutil
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
root = '/Game/Aurelion/UI/HUD/'
edit = unreal.MaterialEditingLibrary
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()

plate = r'''
float2 p=UV;
float aa=max(fwidth(p.y),.002);
// Derivatives keep bevels and symbols isotropic at every native region aspect.
float aspect=max(fwidth(p.y)/max(fwidth(p.x),.00001),1.);
float halfW=.485;
float dy=abs(p.y-.59);
float dist=max(dy-.235,(abs(p.x-.5)-halfW)*aspect+max(dy-.075,0));
float body=1-smoothstep(0,aa,dist);
float rim=(1-smoothstep(.006,.006+aa,abs(dist+.018)))*body;
float innerBevel=exp(-abs(dist+.055)*65)*body;
float capD=max(abs(p.y-.102)-.095,(abs(p.x-.5)-.235)*aspect+max(abs(p.y-.102)-.035,0));
float cap=1-smoothstep(0,aa,capD);
float capRim=(1-smoothstep(.004,.004+aa,abs(capD+.011)))*cap;
float shelfD=max(abs(p.y-.90)-.085,(abs(p.x-.5)-.245)*aspect+max(abs(p.y-.90)-.025,0));
float shelf=1-smoothstep(0,aa,shelfD);
float shelfRim=(1-smoothstep(.004,.004+aa,abs(shelfD+.01)))*shelf;
float shieldD=max(abs(p.y-.25)-.067,(abs(p.x-.5)-.442)*aspect);
float shield=1-smoothstep(0,aa,shieldD);
float shieldRim=(1-smoothstep(.003,.003+aa,abs(shieldD+.008)))*shield;
float2 h=(p-float2(.068,.565))*float2(aspect,1);
float crossD=min(max(abs(h.x)-.036,abs(h.y)-.125),max(abs(h.x)-.125,abs(h.y)-.036));
float medical=1-smoothstep(0,aa,crossD);
float2 s=(p-float2(.068,.25))*float2(aspect,1);
float sw=.065*saturate((.092-s.y)/.065);
float shieldGlyph=1-smoothstep(0,aa,max(abs(s.x)-sw,max(-.065-s.y,s.y-.092)));
float glyph=max(medical,shieldGlyph);
float glass=saturate(body+cap+shelf+shield);
float outerHalo=exp(-max(dist,0)*38)*(1-body)*step(.30,p.y);
float capHalo=exp(-max(capD,0)*70)*(1-cap)*step(p.y,.19);
float upperBevel=exp(-abs(p.y-.37)*80)*body;
float brightness=.003*glass+rim*.86+innerBevel*.15+upperBevel*.12;
brightness+=capRim*.40+shelfRim*.44+shieldRim*.34+glyph*.98;
brightness+=outerHalo*.35+capHalo*.14;
return float4(brightness.xxx,saturate(glass*.97+glyph+outerHalo*.62+capHalo*.30));
'''

arc = r'''
float x=saturate(UV.x);
float curve=.54+.92*x-1.20*x*x;
float d=abs(UV.y-curve);
float aa=max(fwidth(UV.y),.002);
float h=min(.145,min(x,1-x)*4.0);
float split=smoothstep(.018,.023,abs(x-.5));
float body=(1-smoothstep(h,h+aa,d))*split;
float rim=(1-smoothstep(.007,.007+aa,abs(d-h+.018)))*body;
float inset=(1-smoothstep(.086,.086+aa,d))*body;
float cell=smoothstep(.025,.036,frac(x*8));
float ready=step(1-abs(x-.5)*2,saturate(Fill));
float energy=.35+.50*(1-smoothstep(0,.086,d));
float lit=inset*cell*lerp(.018,energy,ready);
float bevel=exp(-abs(d-h+.044)*65)*body*.13;
float cellGlint=exp(-abs(UV.y-curve+.057)*110)*inset*cell*ready*.18;
float chevronX=abs(x-.5);
float chev=(1-smoothstep(.003,.005,abs(chevronX-(.012+abs(UV.y-curve)*.075))))*step(d,.077);
float fine=(1-smoothstep(.004,.004+aa,abs(UV.y-curve-.19)))*.22;
float halo=exp(-max(d-h,0)*36)*(1-body)*split;
float brightness=.003*body+rim*.84+bevel+lit+cellGlint+chev*.96+fine*.55+halo*.18;
return float4(brightness.xxx,saturate(body*.97+chev+fine*.55+halo*.40));
'''

report = dict(status='running', materials=[], layout_changed=False, gameplay_bindings_changed=False)
for name, code, marker in [('M_SovPlateHousing',plate,'Optical glass:'),
                            ('M_SovEchoSegmentedArc',arc,'float glass=body*.97;')]:
    material=unreal.load_asset(root+name)
    visited=set(); shapes=[]
    def visit(node):
        if not node or node.get_path_name() in visited: return
        visited.add(node.get_path_name())
        if isinstance(node,unreal.MaterialExpressionCustom): shapes.append(node)
        for upstream in edit.get_inputs_for_material_expression(material,node): visit(upstream)
    visit(edit.get_material_property_input_node(material,unreal.MaterialProperty.MP_EMISSIVE_COLOR))
    assert len(shapes)==1,name
    shape=shapes[0]; before=shape.get_editor_property('code')
    own_revision = 'float aspect=max(fwidth(p.y)' if name=='M_SovPlateHousing' else 'float cellGlint='
    assert marker in before or own_revision in before,'Unexpected material revision: '+name
    disk=Path(unreal.Paths.project_dir())/'Content/Aurelion/UI/HUD'/(name+'.uasset')
    backup=out/(name+'.before.uasset')
    assert not backup.exists()
    shutil.copy2(disk,backup)
    (out/(name+'.before.hlsl')).write_text(before)
    shape.set_editor_property('code',code)
    edit.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material,only_if_is_dirty=False)
    (out/(name+'.after.hlsl')).write_text(code)
    report['materials'].append(material.get_path_name())
report['status']='saved_requires_visual_review'
(out/'reference-bevels.json').write_text(json.dumps(report,indent=2))
