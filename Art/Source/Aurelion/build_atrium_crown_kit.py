"""Open Aurelion rib crown and carved ring piers, authored at final scale."""
from pathlib import Path
helpers=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helpers.read_text().split('# Four metre bay:')[0],str(helpers),'exec'))
ROOT=Path(__file__).resolve().parent/'AtriumCrownKit';ROOT.mkdir(exist_ok=True)
grout=material('M_Aurelion_StoneGrout',(.30,.275,.23),0,.88)
box('Dressed footing',(0,0,.14),(1.2,1.2,.28),stone,.025)
box('Plinth core',(0,0,.85),(1.02,1.02,1.14),stone,.022)
box('Plinth cornice',(0,0,1.50),(1.18,1.18,.16),stone,.016)
box('Shaft mortar core',(0,0,4.55),(.70,.70,5.94),grout,.008)
for i in range(7):box('Dressed shaft drum',(0,0,1.98+i*.78),(.80,.80,.76),stone,.012)
for front in (-1,1):
 for x in (-.28,.28):
  box('Raised shaft arris',(x,front*.413,4.45),(.065,.040,5.20),stone,.007)
 box('Flute bed',(0,front*.404,4.45),(.065,.008,4.88),grout,0)
 box('Inlaid shaft conductor',(0,front*.410,4.45),(.020,.006,4.64),gold,0)
 for y in (-.28,.28):box('Side arris',(front*.413,y,4.45),(.040,.065,5.20),stone,.007)
for z,w,h in ((7.15,.92,.18),(7.36,1.10,.22),(7.61,1.28,.26),(7.87,1.40,.26)):
 box('Layered crown capital',(0,0,z),(w,w,h),stone,.018)
pier=export('SM_Aurelion_KIT_AtriumCrownPier',[1.4,1.4,8]);hulls=[]
for i,(loc,size) in enumerate((((0,0,.79),(1.20,1.20,1.58)),((0,0,4.34),(.86,.86,5.52)),((0,0,7.55),(1.40,1.40,.90)))):
 h=box(f'UCX_{pier.name}_{i:02}',loc,size,dark,0);h.hide_render=True;h.display_type='WIRE';hulls.append(h)
bpy.ops.object.select_all(action='DESELECT')
for h in hulls:h.select_set(True)
bpy.context.view_layer.objects.active=hulls[0];bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
pier.select_set(True);bpy.context.view_layer.objects.active=pier
bpy.ops.export_scene.fbx(filepath=str(ROOT/(pier.name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',apply_unit_scale=True,bake_anim=False,add_leaf_bones=False,mesh_smooth_type='FACE')
parts.clear();manifest[-1].update(convex_hulls=3,collision='Authored plinth, shaft and capital hulls',pier_collision_samples=[[0,.5,True],[.58,.5,True],[.65,.5,False],[.40,3,True],[.50,3,False],[.68,7.6,True],[.75,7.6,False],[0,8.1,False]]);pier.hide_render=True

def curve(theta,y):return (9.7+22.6*math.sin(theta),y,8+6*math.cos(theta))
for i in range(8):
 start=len(parts)
 path('Continuous arch core',[curve(j*math.pi/2/64,-.45) for j in range(65)],.90,.90,grout,.005)
 for j in range(24):
  a=j*math.pi/48+.0007;b=(j+1)*math.pi/48-.0007
  path('Cut crown voussoir',[curve(a+(b-a)*k/4,-.50) for k in range(5)],.84,1.0,stone,.006)
 for side in (-1,1):
  y=-.514 if side<0 else .502
  path('Restrained arch conductor',[curve(j*math.pi/2/64,y) for j in range(65)],.022,.012,gold,0)
 for o in parts[start:]:o.rotation_euler.z=math.radians(22.5+i*45)

def ring_segment(name,a,b,r0,r1,z0,z1,mat,bevel=.005):
 count=8;angles=[a+(b-a)*j/count for j in range(count+1)];n=len(angles)
 vertices=[(r*math.cos(t),r*math.sin(t),z) for z in (z0,z1) for r in (r0,r1) for t in angles];faces=[]
 for j in range(n-1):faces.extend([(j,j+1,n+j+1,n+j),(2*n+j,3*n+j,3*n+j+1,2*n+j+1),(j,2*n+j,2*n+j+1,j+1),(n+j,n+j+1,3*n+j+1,3*n+j)])
 faces.extend([(0,n,3*n,2*n),(n-1,3*n-1,4*n-1,2*n-1)])
 m=bpy.data.meshes.new(name);m.from_pydata(vertices,[],faces);m.update();o=bpy.data.objects.new(name,m);scene.collection.objects.link(o);finish(o,mat,bevel)
for i in range(32):
 a=i*math.pi/16+.0004;b=(i+1)*math.pi/16-.0004
 ring_segment('Oculus ashlar',a,b,9.12,10.26,13.62,14.36,stone)
 ring_segment('Oculus lower cornice',a,b,9.0,10.36,13.40,13.60,stone)
 ring_segment('Oculus crown coping',a,b,9.0,10.36,14.38,14.58,stone)
 ring_segment('Oculus conductor',a,b,9.06,9.08,14.585,14.59,gold,0)
coords=[o.matrix_world@v.co for o in parts for v in o.data.vertices]
# Update matrices after the radial rotations before deriving export dimensions.
bpy.context.view_layer.update();coords=[o.matrix_world@v.co for o in parts for v in o.data.vertices]
crown=export('SM_Aurelion_KIT_AtriumOpenCrown',[max(v[i] for v in coords)-min(v[i] for v in coords) for i in range(3)])
manifest[-1].update(collision='None; overhead scenic ribs, separate physical piers',clear_oculus_diameter_m=18,minimum_height_m=min(v.z for v in coords))
for i in range(8):
 a=math.radians(22.5+i*45);o=pier.copy();o.data=pier.data;scene.collection.objects.link(o);o.hide_render=False;o.location=(32.3*math.cos(a),32.3*math.sin(a),0);o.rotation_euler.z=a
scene.world=bpy.data.worlds.new('Crown studio');scene.world.color=(.18,.18,.18)
for pos,energy,size in (((10,-25,45),30000,25),((-30,10,25),22000,20)):
 d=bpy.data.lights.new('Crown softbox','AREA');d.energy=energy;d.size=size;l=bpy.data.objects.new('Crown softbox',d);scene.collection.objects.link(l);l.location=pos;l.rotation_euler=(Vector((0,0,8))-l.location).to_track_quat('-Z','Y').to_euler()
d=bpy.data.cameras.new('Crown review');camera=bpy.data.objects.new('Crown review',d);scene.collection.objects.link(camera);scene.camera=camera;camera.location=(63,-70,43);camera.rotation_euler=(Vector((0,0,6))-camera.location).to_track_quat('-Z','Y').to_euler();d.lens=43
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.resolution_x=1600;scene.render.resolution_y=1100;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'Crown-assembly.png')
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Candidate open crown; editor fit and collision qualification pending',modules=manifest),indent=2))
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion_AtriumCrownKit.blend'));bpy.ops.render.render(write_still=True)
