"""Read-only FBX triangle/UV conditioning audit for the inherited vault shell."""
import bpy,json
from pathlib import Path
root=Path(__file__).resolve().parent/'Z08VaultKit'
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(root/'SM_Aurelion_KIT_Z08VaultShell.fbx'))
o=next(o for o in bpy.context.scene.objects if o.type=='MESH');m=o.data;m.calc_loop_triangles();uv=m.uv_layers[0].data
bad=[]
for tri in m.loop_triangles:
    a,b,c=[uv[i].uv.copy() for i in tri.loops];u=b-a;v=c-a;det=u.x*v.y-u.y*v.x
    if abs(det)<1e-10 or tri.area<1e-10:
        bad.append(dict(polygon=tri.polygon_index,area=tri.area,uv_determinant=det,vertices=[list(m.vertices[i].co) for i in tri.vertices]))
(root/'tangent-audit.json').write_text(json.dumps(dict(triangles=len(m.loop_triangles),degenerate_triangles=bad),indent=2))
print('Z08_SHELL_UV_AUDIT',len(bad))
