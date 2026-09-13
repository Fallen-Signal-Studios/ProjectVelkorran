"""Read-only matching skeleton and physics-body audit for saved Eclipse presentation."""
import json
import os
import re
from pathlib import Path
import unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])/'eclipse-collision.json'
report={'roles':{},'errors':[],'combat_qualified':False}
for role in ('Linkbound','WallRunner','Weaver','Elite'):
    try:
        appearance=unreal.load_asset('/Game/Aurelion/Art/Characters/Appearances/CA_Aurelion'+role)
        attrs=appearance.get_editor_property('character_attributes')
        mesh=attrs.get_editor_property('base_mesh')
        physics=mesh.get_editor_property('physics_asset')
        montage=unreal.load_asset('/Game/Aurelion/Enemies/Animation/AM_Eclipse'+role+'_Attack')
        exported=out.parent/(role+'-physics.t3d')
        task=unreal.AssetExportTask()
        for key,value in {'object':physics,'exporter':unreal.ObjectExporterT3D(),'filename':str(exported),
                          'automated':True,'prompt':False,'replace_identical':True}.items():
            task.set_editor_property(key,value)
        assert unreal.Exporter.run_asset_export_task(task)
        bodies=re.findall(r'^\s*BoneName="?([^"\r\n]+)',exported.read_text(encoding='utf-8-sig'),re.MULTILINE)
        assert bodies, 'No body bones in physics asset export'
        report['roles'][role]={'mesh':mesh.get_path_name(),'physics':physics.get_path_name(),
            'physics_body_bones':bodies,'matching_montage_skeleton':montage.get_editor_property('skeleton')==mesh.get_editor_property('skeleton')}
    except Exception:
        import traceback
        report['errors'].append(traceback.format_exc())
out.write_text(json.dumps(report,indent=2),encoding='utf8')
