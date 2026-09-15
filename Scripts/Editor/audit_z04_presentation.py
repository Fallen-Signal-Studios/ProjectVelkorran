"""Read-only relay room annotations, guidance and obsolete decoration inventory."""
from pathlib import Path
import json,os,unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
code=(root/'Scripts/Editor/audit_z03_presentation.py').read_text().replace('5800<p.x<8200 and -20000<p.y<-14800 and -100<p.z<1000','3800<p.x<10200 and -13000<p.y<-9000 and -100<p.z<1000').replace("startswith('Z03__')","startswith('Z04__')").replace("out/'z03-presentation.json'","out/'z04-presentation.json'")
exec(compile(code,'z04_presentation_inventory','exec'),globals())
room=json.loads((root/'Art/Source/Aurelion/Z04WallKit/room-baseline.json').read_text())
old=next(r for r in room['components'] if r['actor']=='Aurelion_Art_M12_Z04_19_63e4a1')
a=next(a for a in actors if a.get_actor_label()==old['actor']);c=a.get_component_by_class(unreal.InstancedStaticMeshComponent)
assert c.static_mesh.get_path_name()==old['mesh']
assert [c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]==old['all_instance_transforms']
(out/'guidance-baseline.json').write_text(json.dumps(old,indent=2))
