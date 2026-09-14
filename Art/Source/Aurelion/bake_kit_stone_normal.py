"""Bake a periodic, low-amplitude stone normal from authored Blender surface geometry."""
import bpy,math,json
from pathlib import Path
root=Path(__file__).resolve().parent/'StoneNormalKit';root.mkdir(exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True);scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=8
waves=[(3,5,.00008,.3),(7,-11,.00005,1.1),(17,13,.000025,2.7),(31,-23,.000012,.7),(53,41,.000006,1.9),(83,-67,.000003,2.2)]
n=256;verts=[]
for j in range(n+1):
    for i in range(n+1):
        x=i/n;y=j/n;z=sum(a*math.sin(2*math.pi*(fx*x+fy*y)+phase) for fx,fy,a,phase in waves)
        verts.append((x,y,z))
faces=[(j*(n+1)+i,j*(n+1)+i+1,(j+1)*(n+1)+i+1,(j+1)*(n+1)+i) for j in range(n) for i in range(n)]
mesh=bpy.data.meshes.new('Periodic shallow stone surface');mesh.from_pydata(verts,[],faces);mesh.update();high=bpy.data.objects.new('Stone microrelief source',mesh);scene.collection.objects.link(high)
for p in mesh.polygons:p.use_smooth=True
lowmesh=bpy.data.meshes.new('Unit metre bake plane');lowmesh.from_pydata([(0,0,0),(1,0,0),(1,1,0),(0,1,0)],[],[(0,1,2,3)]);lowmesh.update();low=bpy.data.objects.new('Stone tangent normal target',lowmesh);scene.collection.objects.link(low)
uv=lowmesh.uv_layers.new(name='Surface_1m')
for loop in lowmesh.loops:uv.data[loop.index].uv=lowmesh.vertices[loop.vertex_index].co.xy
material=bpy.data.materials.new('Normal bake target');material.use_nodes=True;low.data.materials.append(material)
image=bpy.data.images.new('T_AurelionKit_StoneFine_N',width=1024,height=1024,alpha=False);image.colorspace_settings.name='Non-Color';node=material.node_tree.nodes.new('ShaderNodeTexImage');node.image=image;material.node_tree.nodes.active=node
bpy.ops.object.select_all(action='DESELECT');high.select_set(True);low.select_set(True);bpy.context.view_layer.objects.active=low
scene.render.bake.use_selected_to_active=True;scene.render.bake.cage_extrusion=.003;scene.render.bake.max_ray_distance=.006;scene.render.bake.margin=16
bpy.ops.object.bake(type='NORMAL',normal_space='TANGENT')
image.filepath_raw=str(root/'T_AurelionKit_StoneFine_N.png');image.file_format='PNG';image.save()
(root/'manifest.json').write_text(json.dumps(dict(status='Baked source; Unreal material comparison pending',texture='T_AurelionKit_StoneFine_N',resolution=[1024,1024],space='Tangent, Blender OpenGL +Y; flip green on Unreal import',tile_metres=1,source_grid=256,max_absolute_height_bound_m=sum(w[2] for w in waves),periodic_wave_parameters=waves,qualification='Deterministic shallow microrelief; not a scanned material or final AAA surface.'),indent=2))
bpy.ops.wm.save_as_mainfile(filepath=str(root/'Aurelion-StoneNormal.blend'))
print('STONE_NORMAL_BAKE_PASS')
