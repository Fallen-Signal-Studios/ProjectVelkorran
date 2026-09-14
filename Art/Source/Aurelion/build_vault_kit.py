"""Z01 fitted pointed vault: four-metre repeat, editable stone courses and ribs."""
from pathlib import Path
helpers = Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helpers.read_text().split('# Four metre bay:')[0], str(helpers), 'exec'))
ROOT = Path(__file__).resolve().parent / 'VaultKit'
ROOT.mkdir(exist_ok=True)

# Local origin is the spring line (world Z695 cm in Z01), centered across the hall.
profile = [(-12.45,0),(-10,1.55),(-6,2.95),(0,4.4),(6,2.95),(10,1.55),(12.45,0)]

def course(name, a, b, y, length, depth, mat, bevel=.012):
    x1,z1=a; x2,z2=b
    span=math.hypot(x2-x1,z2-z1)
    obj=box(name,((x1+x2)/2,y,(z1+z2)/2),(span,length,depth),mat,bevel)
    obj.rotation_euler[1]=-math.atan2(z2-z1,x2-x1)
    return obj

for seg,(a,b) in enumerate(zip(profile,profile[1:])):
    # Backing seals fine masonry joints without a light leak through the roof.
    course('Continuous vault backing', (a[0],a[1]+.15),(b[0],b[1]+.15),0,4,.18,dark)
    subdivisions=max(2,round(math.dist(a,b)/1.0))
    for i in range(subdivisions):
        t0=i/subdivisions+.002; t1=(i+1)/subdivisions-.002
        p=tuple(a[j]+(b[j]-a[j])*t0 for j in range(2))
        q=tuple(a[j]+(b[j]-a[j])*t1 for j in range(2))
        for y in (-1,1):
            course('Fitted vault ashlar',p,q,y,1.978,.22,stone,.018)
            # Smaller inset intrados panels make thickness and construction legible below.
            inset_p=(p[0]+(q[0]-p[0])*.1,p[1]+(q[1]-p[1])*.1-.125)
            inset_q=(q[0]-(q[0]-p[0])*.1,q[1]-(q[1]-p[1])*.1-.125)
            course('Intrados coffer face',inset_p,inset_q,y,1.65,.06,stone,.012)
    # One structural rib per repeat, with stone fillets flanking a recessed gold rail.
    for y in (-1.87,1.87):
        course('Transverse structural rib',(a[0],a[1]-.14),(b[0],b[1]-.14),y,.25,.3,stone)
        course('Rib recessed channel',(a[0],a[1]-.3),(b[0],b[1]-.3),y,.11,.025,dark,.003)
        course('Rib information rail',(a[0],a[1]-.318),(b[0],b[1]-.318),y,.025,.018,gold,.003)
for x,z in profile[1:-1]:
    box('Longitudinal ridge channel',(x,0,z-.19),(.15,3.98,.08),dark)
    for dx in (-.052,.052):
        box('Ridge conductor',(x+dx,0,z-.235),(.018,3.98,.025),gold,.003)
    for y in (-1,1):
        box('Ridge stone clasp',(x,y,z-.21),(.32,.22,.2),stone,.016)
vault=export('SM_Aurelion_KIT_Vault_25x4',[25,4,5])
manifest[-1]['nominal_dimensions_m']=list(vault.dimensions)
manifest[-1]['placement']='Centered on Z01 X; spring line Z695 cm; four-metre Y repeat; visual only'
manifest[-1]['collision']='None: overhead visual module, retained campaign enclosure collision pending review'
assert len(vault.data.uv_layers)==2
assert all(math.isfinite(c) for v in vault.data.vertices for c in v.co)
assert min(p.area for p in vault.data.polygons)>1e-12

# Closed tympanum for each hall end, filling only the space above the spring line.
for a,b in zip(profile,profile[1:]):
    x1,z1=a; x2,z2=b
    verts=[(x1,y,0) for y in (-.15,.15)]+[(x2,y,0) for y in (-.15,.15)]
    verts += [(x2,y,max(z2,.015)) for y in (-.15,.15)]+[(x1,y,max(z1,.015)) for y in (-.15,.15)]
    data=bpy.data.meshes.new('Tympanum cut stone'); data.from_pydata(verts,[],[(0,2,4,6),(1,7,5,3),(0,1,3,2),(2,3,5,4),(4,5,7,6),(6,7,1,0)]); data.update()
    o=bpy.data.objects.new('Tympanum cut stone',data); scene.collection.objects.link(o); finish(o,stone,.008)
    course('Tympanum arch reveal',a,b,-.17,.035,.13,dark,.004)
    course('Tympanum arch rail',a,b,-.20,.025,.03,gold,.003)
for x in (-10,-6,0,6,10):
    z=next(z for px,z in profile if px==x)
    box('Tympanum vertical joint',(x,-.16,z/2),(.055,.04,max(z-.12,.1)),dark,.004)
    box('Tympanum gold spine',(x,-.188,z/2),(.016,.025,max(z-.18,.1)),gold,.003)
box('Tympanum spring cornice',(0,0,.1),(24.9,.5,.2),stone,.015)
endcap=export('SM_Aurelion_KIT_VaultTympanum_25m',[25,.5,4.5])
manifest[-1]['nominal_dimensions_m']=list(endcap.dimensions)
manifest[-1]['collision']='None; upper visual end closure only'
endcap.hide_render=True

# Look upward along a short assembly to review the visible intrados rather than its back.
for y in (-4,4):
    o=vault.copy(); o.data=vault.data; scene.collection.objects.link(o); o.location.y=y
scene.world=bpy.data.worlds.new('Vault studio'); scene.world.color=(.15,.15,.15)
for pos,energy,size in (((0,0,-5),4500,8),((-9,-3,-2),2800,5),((9,2,-1),2800,5)):
    data=bpy.data.lights.new('Intrados review light','AREA'); data.energy=energy; data.size=size
    o=bpy.data.objects.new('Intrados review light',data); scene.collection.objects.link(o); o.location=pos
    o.rotation_euler=(Vector((0,0,3))-o.location).to_track_quat('-Z','Y').to_euler()
data=bpy.data.cameras.new('Vault review'); camera=bpy.data.objects.new('Vault review',data); scene.collection.objects.link(camera)
camera.location=(0,-11,-5.3); camera.rotation_euler=(Vector((0,0,2))-camera.location).to_track_quat('-Z','Y').to_euler()
data.lens=23; scene.camera=camera
scene.render.engine='CYCLES'; scene.cycles.samples=32; scene.cycles.use_denoising=True
scene.render.resolution_x=1600; scene.render.resolution_y=1000; scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG'; scene.render.filepath=str(ROOT/'Vault-intrados.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion_VaultKit.blend'))
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Fitted source candidate; Unreal room review pending',modules=manifest),indent=2))
bpy.ops.render.render(write_still=True)
