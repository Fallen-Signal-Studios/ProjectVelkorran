"""Z01 stone bridge fitted to measured deck heights; authored segmented collision."""
from pathlib import Path
helpers=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helpers.read_text().split('# Four metre bay:')[0],str(helpers),'exec'))
ROOT=Path(__file__).resolve().parent/'BridgeKit'; ROOT.mkdir(exist_ok=True)
source=ROOT/'measured-profile.json'
profile=json.loads(source.read_text())['profile'] # Blender Y is opposite Unreal Y.

def simplify(points,tolerance=.015):
    if len(points)<3:return points
    y0,z0=points[0]; y1,z1=points[-1]
    errors=[abs(z-(z0+(z1-z0)*(y-y0)/(y1-y0))) for y,z in points]
    index=max(range(len(points)),key=lambda i:errors[i])
    if errors[index]<=tolerance:return [points[0],points[-1]]
    return simplify(points[:index+1],tolerance)[:-1]+simplify(points[index:],tolerance)
collision_profile=simplify(profile)

def prism(name,x,width,a,b,lower,upper,mat,bevel=.006):
    y0,z0=a; y1,z1=b
    vertices=[(px,y,z+dz) for y,z in (a,b) for dz in (lower,upper) for px in (x-width/2,x+width/2)]
    faces=[(0,4,5,1),(2,3,7,6),(0,2,6,4),(1,5,7,3),(0,1,3,2),(4,6,7,5)]
    data=bpy.data.meshes.new(name);data.from_pydata(vertices,[],faces);data.update()
    o=bpy.data.objects.new(name,data);scene.collection.objects.link(o)
    return finish(o,mat,bevel)

for a,b in zip(profile,profile[1:]):
    prism('Continuous stone deck',0,4.2,a,b,-.19,-.012,stone)
    # Separate fitted wearing courses, with narrow longitudinal joints.
    gap=.004
    dy=b[0]-a[0];slope=(b[1]-a[1])/dy
    pa=(a[0]+gap,a[1]+gap*slope);pb=(b[0]-gap,b[1]-gap*slope)
    for x in (-1.05,1.05):prism('Honed walking course',x,2.092,pa,pb,-.022,0,stone,.003)
    for x in (-2.17,2.17):
        prism('Balustrade stone plinth',x,.16,a,b,-.14,.16,stone)
        prism('Recessed side reveal',x,.185,a,b,-.08,-.025,dark,.003)
        prism('Stone handrail',x,.19,a,b,.97,1.1,stone,.012)
        prism('Handrail gold conductor',x,.205,a,b,1.022,1.041,gold,.003)
        prism('Intermediate guard rail',x,.06,a,b,.49,.55,gold,.005)
for index,(y,z) in enumerate(profile):
    if index%3 and index not in (0,len(profile)-1):continue
    for x in (-2.17,2.17):
        box('Balustrade upright',(x,y,z+.56),(.19,.22,.97),stone,.012)
        for h in (.22,.88):box('Upright collar',(x,y,z+h),(.23,.27,.065),stone,.007)
        box('Upright axial channel',(x+.102,y,z+.57),(.018,.044,.56),dark,.003)
visual=export('SM_Aurelion_KIT_Z01_StoneBridge',[4.6,33,3.3])
manifest[-1]['nominal_dimensions_m']=list(visual.dimensions)
name=visual.name; hulls=[]
def collision(x,width,a,b,lower,upper):
    hull=prism('UCX_'+name+'_'+str(len(hulls)).zfill(2),x,width,a,b,lower,upper,dark,0)
    hull.hide_render=True;hull.display_type='WIRE';hulls.append(hull)
for a,b in zip(collision_profile,collision_profile[1:]):
    collision(0,4.2,a,b,-.19,0)
    for x in (-2.17,2.17):
        collision(x,.185,a,b,-.14,.16)
        collision(x,.205,a,b,.97,1.1)
        collision(x,.06,a,b,.49,.55)
for index,(y,z) in enumerate(profile):
    if index%3 and index not in (0,len(profile)-1):continue
    for x in (-2.17,2.17):collision(x,.23,(y-.135,z),(y+.135,z),.075,1.045)
parts.clear()
bpy.ops.object.select_all(action='DESELECT');visual.select_set(True)
for hull in hulls:hull.select_set(True)
bpy.context.view_layer.objects.active=visual
bpy.ops.export_scene.fbx(filepath=str(ROOT/(name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',apply_unit_scale=True,bake_anim=False,add_leaf_bones=False,mesh_smooth_type='FACE')
manifest[-1].update(convex_hulls=len(hulls),collision='Segmented deck and balustrade hulls; measured profile fit tolerance 1.5 cm',
    deck_samples=profile,rail_x=2.17,rail_height=1.1,world_origin_cm=[-6164.686518,-14700,0])
scene.world=bpy.data.worlds.new('Bridge studio');scene.world.color=(.12,.12,.12)
for pos in ((-7,-10,12),(8,10,12)):
    d=bpy.data.lights.new('Bridge softbox','AREA');d.energy=3500;d.size=12
    o=bpy.data.objects.new('Bridge softbox',d);scene.collection.objects.link(o);o.location=pos;o.rotation_euler=(Vector((0,0,1))-o.location).to_track_quat('-Z','Y').to_euler()
d=bpy.data.cameras.new('Bridge review');camera=bpy.data.objects.new('Bridge review',d);scene.collection.objects.link(camera)
camera.location=(23,-27,17);camera.rotation_euler=(Vector((0,0,1))-camera.location).to_track_quat('-Z','Y').to_euler();d.lens=36;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True
scene.render.resolution_x=1600;scene.render.resolution_y=900;scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG';scene.render.filepath=str(ROOT/'Stone-bridge.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion_BridgeKit.blend'))
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Measured bridge candidate; rail-height change and live traversal require review',modules=manifest),indent=2))
bpy.ops.render.render(write_still=True)
