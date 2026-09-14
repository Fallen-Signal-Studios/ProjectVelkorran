"""Shallow, tapered Eclipse intrusion with fractured stone lips; metres."""
from pathlib import Path
import random
helper=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helper.read_text().split('# Four metre bay:')[0],str(helper),'exec'))
ROOT=Path(__file__).resolve().parent/'EclipseWallKit';ROOT.mkdir(exist_ok=True)
black=material('M_Eclipse_Intrusion',(.008,.006,.012),.05,.31)
violet=material('M_Eclipse_Seam',(.095,.009,.19),0,.5)
shader=violet.node_tree.nodes.get('Principled BSDF')
shader.inputs['Emission Color'].default_value=(.15,.012,.28,1)
shader.inputs['Emission Strength'].default_value=.5

def ribbon(name,points,width,depth,mat,seed):
    rng=random.Random(seed);vertices=[]
    for i,(x,z) in enumerate(points):
        prev=Vector(points[max(0,i-1)]);nxt=Vector(points[min(len(points)-1,i+1)])
        d=(nxt-prev).normalized();n=Vector((-d.y,d.x))
        taper=max(.06,math.sin(math.pi*i/(len(points)-1))**.65)
        w=width*taper*rng.uniform(.75,1.2)
        # Four-point cross section: embedded back and shallow convex front.
        vertices.extend([(x-n.x*w,0,z-n.y*w),(x+n.x*w,0,z+n.y*w),
                         (x+n.x*w*.6,-depth*taper,z+n.y*w*.6),
                         (x-n.x*w*.6,-depth*taper,z-n.y*w*.6)])
    faces=[(3,2,1,0)]
    for i in range(len(points)-1):
        for j in range(4):faces.append((i*4+j,i*4+(j+1)%4,(i+1)*4+(j+1)%4,(i+1)*4+j))
    last=(len(points)-1)*4;faces.append(tuple(last+j for j in range(4)))
    data=bpy.data.meshes.new(name);data.from_pydata(vertices,[],faces);data.update()
    obj=bpy.data.objects.new(name,data);scene.collection.objects.link(obj);finish(obj,mat,.0007)

for variant in range(2):
    rng=random.Random(731+variant)
    points=[(.09*math.sin(i*.35+variant)+rng.uniform(-.04,.04),.05+i*.075) for i in range(21)]
    ribbon('Branching dark intrusion',points,.028,.012,black,variant)
    for i in (3,6,10,13,17):
        x,z=points[i];side=rng.choice((-1,1))
        branch=[(x,z),(x+side*.09,z+.035),(x+side*.15,z+.11),(x+side*.29,z+.14),(x+side*.39,z+.21)]
        ribbon('Tapered secondary fracture',branch,.010,.006,black,i+variant)
        for j,(bx,bz) in enumerate(branch[1:-1]):
            ribbon('Hairline tertiary fracture',[(bx,bz),(bx-side*.02,bz+.07),(bx+side*.03,bz+.13)],.003,.003,black,70+i+j)
    for i in range(1,19):
        x,z=points[i]
        # Separate angular chips create shallow, irregular stone lips, not a slab.
        for side in (-1,1):
            size=rng.uniform(.018,.037)
            cx=x+side*rng.uniform(.03,.05);verts=[]
            for y in (0,-.009):
                for j in range(5):
                    a=j*2*math.pi/5;r=size*rng.uniform(.7,1.1)
                    verts.append((cx+math.cos(a)*r,y,z+math.sin(a)*r))
            faces=[(4,3,2,1,0),(5,6,7,8,9)]+[(j,(j+1)%5,(j+1)%5+5,j+5) for j in range(5)]
            data=bpy.data.meshes.new('Angular stone chip');data.from_pydata(verts,[],faces);data.update()
            chip=bpy.data.objects.new('Broken stone lip',data);scene.collection.objects.link(chip);finish(chip,stone,.001)
        if i%3==0:
            short=[(px,-.013,pz) for px,pz in points[i:i+2]]
            path('Interrupted violet capillary',short,.003,.002,violet,.0004)
    import bmesh
    for part in parts:
        bm=bmesh.new();bm.from_mesh(part.data)
        bmesh.ops.dissolve_degenerate(bm,dist=0.000001,edges=list(bm.edges))
        bmesh.ops.delete(bm,geom=[f for f in bm.faces if f.calc_area()<1e-10],context='FACES_ONLY')
        bm.to_mesh(part.data);bm.free();part.data.update()
    obj=export('SM_Aurelion_KIT_EclipseWallScar_'+str(variant+1),[1,.02,1.6])
    manifest[-1]['nominal_dimensions_m']=list(obj.dimensions)
    manifest[-1]['collision']='None; shallow visual overlay only'
    manifest[-1]['position_precision']=10
    obj.hide_render=True
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Source authored; in-engine fit and visual acceptance pending',modules=manifest),indent=2))
for i,obj in enumerate(modules):obj.hide_render=False;obj.location.x=i*1.25
scene.world=bpy.data.worlds.new('Fracture studio');scene.world.color=(.18,.18,.18)
for pos,power,size in [((1,-3,3),500,3),((-2,-1,1),180,2)]:
    bpy.ops.object.light_add(type='AREA',location=pos);o=bpy.context.object;o.data.energy=power;o.data.size=size
    o.rotation_euler=(Vector((.6,0,.8))-o.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(2,-4,2));camera=bpy.context.object
camera.rotation_euler=(Vector((.6,0,.8))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=2.6;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=48;scene.cycles.use_denoising=True
scene.render.resolution_x=1400;scene.render.resolution_y=1000;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'scars.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Eclipse-Wall-Scars.blend'));bpy.ops.render.render(write_still=True)
print('ECLIPSE_WALL_KIT_BUILD_PASS')
