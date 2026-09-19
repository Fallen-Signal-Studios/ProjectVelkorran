"""Deepen HUD optical backings and add restrained edge halos, preserving all inputs.
Does not rebuild widgets or alter gameplay graphs. Run in the isolated editor runner.
"""
import unreal, os, json, shutil
from pathlib import Path
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
root='/Game/Aurelion/UI/HUD/'
edit=unreal.MaterialEditingLibrary
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
patches={
'M_SovPlateHousing':('float brightness = .015*',r'''
// Optical glass: dark interior, narrow luminous trim, soft perimeter bleed.
float glass=saturate(body+cap+shelf+shield);
float halo=exp(-abs(dist+.021)*95)*body;
float outerHalo=exp(-max(dist,0)*85)*(1-body)*step(abs(p.y-.59),.29);
float bevel=exp(-abs(p.y-.375)*85)*body*.09;
float brightness=.003*glass+rim*.82+halo*.10+outerHalo*.13+bevel+glyph*.97+glint*.20;
return float4(brightness.xxx,saturate(glass*.97+glyph+outerHalo*.22));
'''),
'M_SovAmmoHousing':('return float4((body*.018',r'''
float halo=exp(-abs(d+.026)*75)*body;
float outerHalo=exp(-max(d,0)*60)*(1-body);
float brightness=body*.003+rim*.82+halo*.10+outerHalo*.12+cartridge*.97;
return float4(brightness.xxx,saturate(body*.97+cartridge+outerHalo*.20));
'''),
'M_SovRadarReticle':('float brightness = body*.012',r'''
float halo=exp(-abs(r-.83)*100)*.16+exp(-abs(r-.87)*80)*.065;
float glass=body*.96;
float brightness=body*.003+ring*.78+rim*.32+rings*.09+axes*.065+ticks*.95+arrow+sector*.24+beam*.65+halo;
return float4(brightness.xxx,saturate(glass+rim*.65+arrow+halo*.6));
'''),
'M_SovEchoSegmentedArc':('float brightness = .018*',r'''
float halo=exp(-abs(d-h+.016)*95)*.12*split;
float glass=body*.97;
float brightness=.003*body+rim*.80+lit+chev*.96+fine*.65+halo;
return float4(brightness.xxx,saturate(glass+chev+fine*.6+halo*.7));
''')}
report=[]
for name,(marker,ending) in patches.items():
    mat=unreal.load_asset(root+name)
    visited=set(); shapes=[]
    def visit(node):
        if not node or node.get_path_name() in visited:return
        visited.add(node.get_path_name())
        if isinstance(node,unreal.MaterialExpressionCustom):shapes.append(node)
        for upstream in edit.get_inputs_for_material_expression(mat,node):visit(upstream)
    visit(edit.get_material_property_input_node(mat,unreal.MaterialProperty.MP_EMISSIVE_COLOR))
    assert len(shapes)==1,name
    shape=shapes[0];before=shape.get_editor_property('code')
    assert before.count(marker)==1,'Unexpected shader revision: '+name
    source=Path(unreal.Paths.project_dir())/'Content/Aurelion/UI/HUD'/(name+'.uasset')
    shutil.copy2(source,out/(name+'.before.uasset'))
    (out/(name+'.before.hlsl')).write_text(before)
    after=before[:before.index(marker)]+ending
    if name=='M_SovPlateHousing':
        after=after.replace('max(abs(p.x-.5)-halfW,abs(p.y-.59)-.235)','max((abs(p.x-.5)-halfW)*8.333333,abs(p.y-.59)-.235)')
    elif name=='M_SovAmmoHousing':
        after=after.replace('max(max(left-UV.x,UV.x-right),max(.06-UV.y,UV.y-.90))','max(max(left-UV.x,UV.x-right)*3.333333,max(.06-UV.y,UV.y-.90))')
    shape.set_editor_property('code',after)
    edit.recompile_material(mat)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mat,only_if_is_dirty=False)
    (out/(name+'.after.hlsl')).write_text(after)
    report.append(dict(material=name,inputs=[str(i.get_editor_property('input_name')) for i in shape.get_editor_property('inputs')]))
(out/'optical-housings.json').write_text(json.dumps(dict(status='saved_pending_visual_qualification',materials=report),indent=2))
