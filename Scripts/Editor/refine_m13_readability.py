"""Preview or save local rail finish and readable destination lettering."""
from pathlib import Path
import hashlib,json,os,runpy,shutil,unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);lib=unreal.MaterialEditingLibrary
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert not level.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Aurelion_M13'
persist=bool(globals().get('SAVE_M13_READABILITY',False));dest='/Game/Aurelion/Environment/ArchitectureKit/Materials'
rail_path=dest+'/M_AurelionKit_RailBronze';text_path=dest+'/M_AurelionKit_WayfindingText'
if not persist:
    assert not unreal.EditorAssetLibrary.does_asset_exist(rail_path) and not unreal.EditorAssetLibrary.does_asset_exist(text_path),'Use reviewed materials for persistence; do not overwrite a prior authoring pass'
    rail=unreal.EditorAssetLibrary.duplicate_asset(dest+'/M_AurelionKit_Reveal',rail_path)
    base=lib.get_material_property_input_node(rail,unreal.MaterialProperty.MP_BASE_COLOR)
    assert isinstance(base,unreal.MaterialExpressionLinearInterpolate)
    constant,textured,_=lib.get_inputs_for_material_expression(rail,base)
    tinted=lib.get_inputs_for_material_expression(rail,textured)[1]
    assert isinstance(constant,unreal.MaterialExpressionConstant3Vector) and isinstance(tinted,unreal.MaterialExpressionConstant3Vector)
    for node in (constant,tinted):node.set_editor_property('constant',unreal.LinearColor(.18,.15,.105,1))
    base.set_editor_property('const_alpha',.14)
    rough=lib.get_material_property_input_node(rail,unreal.MaterialProperty.MP_ROUGHNESS);assert isinstance(rough,unreal.MaterialExpressionAdd)
    rough.set_editor_property('const_b',.48)
    variation=lib.get_inputs_for_material_expression(rail,rough)[0];variation.set_editor_property('const_b',.12)
    assert lib.get_material_property_input_node(rail,unreal.MaterialProperty.MP_EMISSIVE_COLOR) is None
    lettering=unreal.EditorAssetLibrary.duplicate_asset('/Engine/EngineMaterials/DefaultTextMaterialOpaque',text_path)
    base=lib.get_material_property_input_node(lettering,unreal.MaterialProperty.MP_BASE_COLOR);assert base
    lettering.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    assert lib.connect_material_property(base,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    for material in (rail,lettering):lib.recompile_material(material);assert unreal.EditorAssetLibrary.save_loaded_asset(material)
else:
    rail=unreal.load_asset(rail_path);lettering=unreal.load_asset(text_path);assert rail and lettering
actors=list(sub.get_all_level_actors());helper=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));before=helper['snapshot_actor_state'](actors)
owner=next(a for a in actors if a.get_actor_label()=='Aurelion_Art_M13_Z10_12_de411e')
rails=owner.get_components_by_class(unreal.InstancedStaticMeshComponent);assert len(rails)==4
rail_report=[]
for c in rails:
    slots=c.static_mesh.get_editor_property('static_materials')
    indices=[i for i,s in enumerate(slots) if str(s.get_editor_property('imported_material_slot_name'))=='M_Aurelion_ChannelShadow'];assert len(indices)==1
    i=indices[0];assert c.get_material(i).get_path_name()==dest+'/M_AurelionKit_Reveal.M_AurelionKit_Reveal'
    c.modify();c.set_material(i,rail);rail_report.append(dict(mesh=c.static_mesh.get_name(),slot=i))
labels=['Aurelion_Art_Sign_Z10_72eb71','Aurelion_Art_Sign_Z11_9bb415','Aurelion_Art_Sign_Z12_f79ad4']
text_report=[]
for label in labels:
    actor=next(a for a in actors if a.get_actor_label()==label);c=actor.get_component_by_class(unreal.TextRenderComponent)
    assert c.get_material(0).get_path_name()=='/Engine/EngineMaterials/DefaultTextMaterialOpaque.DefaultTextMaterialOpaque'
    text_report.append(dict(actor=label,text=str(c.text),transform=c.get_world_transform().export_text()))
    c.modify();c.set_text_material(lettering)
assert helper['snapshot_actor_state'](actors)==before
report=dict(status='unsaved_preview',rails=rail_report,signs=text_report,scene_lights_unchanged=True)
if persist:
    m12=root/'Content/Aurelion/Maps/L_Aurelion_M12.umap';protected=hashlib.sha256(m12.read_bytes()).hexdigest()
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M13.umap',out/'L_Aurelion_M13.before.umap')
    assert level.save_current_level() and level.load_level('/Game/Aurelion/Maps/L_Aurelion_M13')
    current=list(sub.get_all_level_actors());assert helper['snapshot_actor_state'](current)==before
    restored=next(a for a in current if a.get_actor_label()=='Aurelion_Art_M13_Z10_12_de411e')
    for c in restored.get_components_by_class(unreal.InstancedStaticMeshComponent):
        row=next(r for r in rail_report if r['mesh']==c.static_mesh.get_name());assert c.get_material(row['slot'])==rail
    for row in text_report:
        c=next(a for a in current if a.get_actor_label()==row['actor']).get_component_by_class(unreal.TextRenderComponent)
        assert c.get_material(0)==lettering and str(c.text)==row['text'] and c.get_world_transform().export_text()==row['transform']
    assert hashlib.sha256(m12.read_bytes()).hexdigest()==protected
    report.update(status='saved_reloaded',m12_unchanged=True)
(out/'m13-readability-fit.json').write_text(json.dumps(report,indent=2))
runpy.run_path(str(root/'Scripts/Editor/preview_m13_route.py'),init_globals=dict(ALLOW_DIRTY_PREVIEW=not persist,
    M13_ROUTE_VIEWS=[('railing',(1700,33200,-1600)),('chamber-sign',(0,32200,-1510)),('gallery-sign',(0,41500,160)),('departure-sign',(0,45900,160))],
    M13_ROUTE_YAWS={'railing':-50},M13_ROUTE_PITCHES={'railing':-8,'chamber-sign':-8}))
