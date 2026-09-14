"""Survivor Bend: measured side vaults, coursed arch faces and central coffers."""
from pathlib import Path
helpers=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helpers.read_text().split('# Four metre bay:')[0],str(helpers),'exec'))
ROOT=Path(__file__).resolve().parent/'Z02VaultKit'; ROOT.mkdir(exist_ok=True)
fit=json.loads((ROOT/'measured-profile.json').read_text())
fine=fit['profile']; profile=[fine[0]]+fine[1:-1:2]+[fine[-1]]
roof_half=fit['roof_y_half_m']; cap=9.77

def course(name,x,width,a,b,offset,thickness,mat,bevel=.008,shorten=0):
    y0,z0=a; y1,z1=b; length=math.hypot(y1-y0,z1-z0)
    o=box(name,(x,(y0+y1)/2,(z0+z1)/2+offset),(width,max(length-shorten,.01),thickness),mat,bevel)
    o.rotation_euler.x=math.atan2(z1-z0,y1-y0)
    return o

def vault(width,name):
    for a,b in zip(profile,profile[1:]):
        course('Continuous vault backing',0,width,a,b,.12,.18,dark,.003)
        for x in (-width/4,width/4):
            course('Vault stone wearing course',x,width/2-.008,a,b,.025,.16,stone,.012,.008)
        if width>1:
            for x in (-width/2+.10,width/2-.10):
                course('Vault structural rib',x,.16,a,b,-.035,.10,stone,.009)
                course('Rib conductor recess',x,.08,a,b,-.092,.014,dark,.002)
                course('Rib conductor',x,.021,a,b,-.102,.012,gold,.002)
    # Flat outer roof closes the space above the curved intrados.
    box('Vault roof closure',(0,0,cap-.06),(width,roof_half*2,.12),stone,.008)
    o=export(name,[width,roof_half*2,cap]); manifest[-1]['nominal_dimensions_m']=list(o.dimensions)
    manifest[-1]['collision']='None; retained inner-shell collision; decorative intrados offsets require live review'
    o.hide_render=True
    return o

bay=vault(2,'SM_Aurelion_KIT_Z02SideVault_2m')
edge_width=fit['outer_x_m']-fit['inner_x_m']-8
edge=vault(edge_width,'SM_Aurelion_KIT_Z02SideVault_Edge')

def prism_yz(name,points,x,depth,mat,bevel=.003):
    n=len(points); verts=[(px,y,z) for px in (x-depth/2,x+depth/2) for y,z in points]
    faces=[tuple(range(n-1,-1,-1)),tuple(range(n,n*2))]
    faces += [(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
    mesh=bpy.data.meshes.new(name); mesh.from_pydata(verts,[],faces); mesh.update()
    o=bpy.data.objects.new(name,mesh); scene.collection.objects.link(o)
    return finish(o,mat,bevel)

# Clip each ashlar course against the measured lower boundary of the arch face.
border=[[-roof_half,.08]]+fine+[[roof_half,.08]]
prism_yz('Continuous arch-face backing',border+[[roof_half,cap],[-roof_half,cap]],0,.24,dark,.002)

def clip_above(poly,a,b):
    y0,z0=a; y1,z1=b
    def distance(p):return p[1]-(z0+(z1-z0)*(p[0]-y0)/(y1-y0))-.025
    result=[]
    for p,q in zip(poly,poly[1:]+poly[:1]):
        dp,dq=distance(p),distance(q)
        if dp>=0:result.append(p)
        if (dp>=0)!=(dq>=0):
            t=dp/(dp-dq); result.append((p[0]+t*(q[0]-p[0]),p[1]+t*(q[1]-p[1])))
    return result

for a,b in zip(border,border[1:]):
    ya,yb=a[0]+.004,b[0]-.004
    if yb-ya<.015:continue
    for row in range(15):
        za=.08+row*.65+.004; zb=min(.08+(row+1)*.65-.004,cap-.006)
        poly=clip_above([(ya,za),(yb,za),(yb,zb),(ya,zb)],a,b)
        area=abs(sum(p[0]*q[1]-q[0]*p[1] for p,q in zip(poly,poly[1:]+poly[:1])))/2 if len(poly)>2 else 0
        if area<.006:continue
        for x in (-.14,.14):prism_yz('Clipped arch ashlar',poly,x,.06,stone,0)

for face in (-1,1):
    # Rotate the common X/Z moulding into this arch's Y/Z plane.
    # Positive-Y extrusion rotates toward negative X: offset denotes the
    # outward face, so each layer must sit beyond the ashlar's +/-0.17 face.
    for width,depth,offset,mat in ((.21,.07,.24,stone),(.075,.025,.265,dark),(.022,.018,.283,gold)):
        start_x=face*offset+(depth if face<0 else 0)
        points=[(y,-start_x,min(z+.09,cap-width/2-.02)) for y,z in fine]
        o=path('Arch intrados moulding',points,width,depth,mat); o.rotation_euler.z=math.pi/2
    box('Arch-face crown cornice',(face*.17,0,cap-.08),(.12,roof_half*2,.16),stone,.012)
    box('Crown conductor',(face*.236,0,cap-.065),(.012,roof_half*2-.05,.022),gold,.002)
spandrel=export('SM_Aurelion_KIT_Z02ArchFace',[.6,roof_half*2,cap])
manifest[-1]['nominal_dimensions_m']=list(spandrel.dimensions)
manifest[-1]['collision']='None; decorative front casing around retained side-vault collision'
spandrel.hide_render=True

# Central ceiling pivot is its lowest intrados plane, unlike floor-based vaults.
width=12.76; length=roof_half*2
box('Central roof backing',(0,0,.19),(width,length,.12),stone,.008)
for ix in range(6):
    for iy in range(10):
        x=-width/2+(ix+.5)*width/6; y=-length/2+(iy+.5)*length/10
        box('Coffer shadow bed',(x,y,.15),(width/6-.018,length/10-.018,.03),dark,.004)
        box('Recessed ceiling stone',(x,y,.11),(width/6-.17,length/10-.17,.08),stone,.013)
for ix in range(7):
    x=-width/2+ix*width/6
    box('Longitudinal ceiling rib',(x,0,.035),(.12,length,.07),stone,.009)
    box('Ceiling information channel',(x,0,.004),(.022,length-.03,.008),gold,.001)
for iy in range(11):
    y=-length/2+iy*length/10
    box('Transverse ceiling rib',(0,y,.045),(width,.11,.09),stone,.01)
ceiling=export('SM_Aurelion_KIT_Z02CentralCeiling',[width+.12,length+.11,.25])
manifest[-1]['nominal_dimensions_m']=list(ceiling.dimensions)
manifest[-1]['collision']='None; retained central ceiling collision'
ceiling.hide_render=True

# Review a two-bay section and its finished face from the central nave.
bay.hide_render=False; bay.location.x=-1.2
o=bay.copy(); o.data=bay.data; scene.collection.objects.link(o); o.location.x=-3.2
spandrel.hide_render=False
scene.world=bpy.data.worlds.new('Side vault studio'); scene.world.color=(.14,.14,.14)
for pos in (((6,-5,7)),((-4,0,3)),((2,8,10))):
    d=bpy.data.lights.new('Vault review light','AREA'); d.energy=2400; d.size=7
    o=bpy.data.objects.new('Vault review light',d); scene.collection.objects.link(o); o.location=pos; o.rotation_euler=(Vector((-1,0,5))-o.location).to_track_quat('-Z','Y').to_euler()
d=bpy.data.cameras.new('Side vault review'); camera=bpy.data.objects.new('Side vault review',d); scene.collection.objects.link(camera)
camera.location=(18,-15,9); camera.rotation_euler=(Vector((-1,0,4.9))-camera.location).to_track_quat('-Z','Y').to_euler(); d.lens=40; scene.camera=camera
scene.render.engine='CYCLES'; scene.cycles.samples=32; scene.cycles.use_denoising=True
scene.render.resolution_x=1600; scene.render.resolution_y=1100; scene.render.resolution_percentage=100; scene.render.image_settings.file_format='PNG'; scene.render.filepath=str(ROOT/'Side-vault-assembly.png')
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Measured side-vault source candidate; room, collision correspondence and surface quality require review',modules=manifest),indent=2))
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion_Z02VaultKit.blend')); bpy.ops.render.render(write_still=True)
