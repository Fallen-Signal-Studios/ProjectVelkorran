"""Fresh-process checks of saved radar contact brushes and HUD graph references."""
import unreal, runpy, os, json
from pathlib import Path
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
runpy.run_path(str(Path(unreal.Paths.project_dir())/'Scripts/Editor/verify_hud_ability_pips.py'))
root='/Game/Aurelion/UI/HUD/'
mat=unreal.load_asset(root+'M_SovRadarContact')
assert mat.get_editor_property('material_domain')==unreal.MaterialDomain.MD_UI
assert mat.get_editor_property('blend_mode')==unreal.BlendMode.BLEND_TRANSLUCENT
for label,value in [('Live',1),('Memory',0)]:
    mi=unreal.load_asset(root+'MI_SovRadarContact'+label)
    assert mi.get_editor_property('parent')==mat
    assert unreal.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(mi,'Live')==value
    brush=unreal.load_asset(root+'SB_SovRadarContact'+label)
    assert brush.get_editor_property('brush').get_editor_property('resource_object')==mi
bp=unreal.load_asset(root+'WBP_SovHolographicHUD')
task=unreal.AssetExportTask()
for k,v in dict(object=bp,exporter=unreal.ObjectExporterT3D(),filename=str(out/'radar-saved-graph.t3d'),automated=True,prompt=False,selected=False,replace_identical=True).items():task.set_editor_property(k,v)
assert unreal.Exporter.run_asset_export_task(task)
text=(out/'radar-saved-graph.t3d').read_text(encoding='utf-8-sig')
for token in ['OnPaint','K2Node_RadarDraw','K2Node_RadarLoop','SB_SovRadarContactMemory.SB_SovRadarContactMemory','SB_SovRadarContactLive.SB_SovRadarContactLive']:
    assert token in text,token
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
(out/'radar-saved.json').write_text(json.dumps(dict(status='passed',scope='Saved UI materials, brush references, paint graph presence and compilation. Runtime and visual checks separate.'),indent=2))
unreal.log('HUD_RADAR_SAVED_VERIFICATION_PASSED')
