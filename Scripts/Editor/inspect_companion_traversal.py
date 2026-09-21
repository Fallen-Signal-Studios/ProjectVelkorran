"""Read-only player/companion traversal chooser and skeleton inventory."""
from pathlib import Path
import json,os,unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);rows=[]
for hero in ('Selene','Tarrik'):
    for role,path in [('player','/Game/PlayerCharacters/BP_Sov'+hero),
                      ('companion','/Game/Aurelion/Characters/BP_Aurelion'+hero+'Companion')]:
        bp=unreal.load_asset(path);assert bp
        cdo=unreal.get_default_object(bp.generated_class())
        table=cdo.get_editor_property('TraversalTable')
        mesh=cdo.get_editor_property('mesh').get_editor_property('skeletal_mesh_asset')
        rows.append(dict(hero=hero,role=role,path=path,table=table.get_path_name() if table else None,
                         mesh=mesh.get_path_name() if mesh else None,
                         skeleton=mesh.get_editor_property('skeleton').get_path_name() if mesh else None))
        task=unreal.AssetExportTask()
        for k,v in dict(object=cdo,exporter=unreal.ObjectExporterT3D(),filename=str(out/(bp.get_name()+'.t3d')),
                        automated=True,prompt=False,replace_identical=False).items():task.set_editor_property(k,v)
        assert unreal.Exporter.run_asset_export_task(task)
table=unreal.load_asset('/NarrativePro/Pro/Core/Character/Biped/Animation/Sequences/GASP/Traversal/CHT_TraversalAnims_Biped')
assert table
task=unreal.AssetExportTask()
for k,v in dict(object=table,exporter=unreal.ObjectExporterT3D(),filename=str(out/'traversal-chooser.t3d'),
                automated=True,prompt=False,replace_identical=False).items():task.set_editor_property(k,v)
assert unreal.Exporter.run_asset_export_task(task)
(out/'companion-traversal.json').write_text(json.dumps(dict(rows=rows,assets_saved=[]),indent=2))
