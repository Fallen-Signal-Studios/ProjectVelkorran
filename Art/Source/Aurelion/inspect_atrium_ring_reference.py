"""Extract the exported ring wedge vertices in metres without editing the reference."""
import bpy,json,sys,math
from pathlib import Path
folder=Path(sys.argv[sys.argv.index('--')+1])
rows=[]
for filename in sorted(folder.glob('SM_RingSector*.fbx')):
 bpy.ops.wm.read_factory_settings(use_empty=True)
 bpy.ops.import_scene.fbx(filepath=str(filename))
 for o in bpy.context.scene.objects:
  if o.type!='MESH' or o.name.startswith('UCX_'):continue
  points=sorted(set(tuple(round(v,7) for v in (o.matrix_world@p.co)) for p in o.data.vertices))
  rows.append(dict(file=filename.name,object=o.name,vertices=points,triangles=sum(len(p.vertices)-2 for p in o.data.polygons),materials=[m.name for m in o.data.materials],radii=sorted(set(round(math.hypot(p[0],p[1]),6) for p in points))))
assert len(rows)==2
(folder/'ring-reference-vertices.json').write_text(json.dumps(rows,indent=2))
print('RING_REFERENCE_EXTRACTED',[(r['object'],len(r['vertices']),r['radii']) for r in rows])
