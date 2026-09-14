"""Carved Aurelion bridge portico with a split coffered barrel vault."""
from pathlib import Path
helpers=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helpers.read_text().split('# Four metre bay:')[0],str(helpers),'exec'))
ROOT=Path(__file__).resolve().parent/'AtriumCanopyKit';ROOT.mkdir(exist_ok=True)
grout=material('M_Aurelion_StoneGrout',(.30,.275,.23),0,.88)
def arch(theta,y,offset=0):return (3.13*math.sin(theta),y,5.2+1.7*math.cos(theta)+offset)
def module(name):
    coords=[o.matrix_world@v.co for o in parts for v in o.data.vertices]
    dims=[max(v[i] for v in coords)-min(v[i] for v in coords) for i in range(3)]
    o=export(name,dims);manifest[-1].update(collision='None; replacement for noncolliding canopy art',walking_clearance_m=3.5);return o
for x in (-3.13,3.13):
    for y in (-3.325,3.325):
        box('Dressed pier footing',(x,y,.10),(.38,.40,.20),stone,.012)
        box('Pier plinth',(x,y,.72),(.34,.36,1.04),stone,.012)
        box('Plinth capital',(x,y,1.28),(.40,.42,.08),stone,.007)
        box('Continuous recessed pier core',(x,y,3.19),(.26,.28,3.74),grout,.005)
        for i in range(5):box('Dressed pier drum',(x,y,1.69+i*.70),(.31,.33,.68),stone,.008)
        for side in (-1,1):
            box('Carved pier arris',(x+side*.122,y-.177,3.14),(.025,.034,3.48),stone,.003)
            box('Carved pier arris',(x+side*.122,y+.177,3.14),(.025,.034,3.48),stone,.003)
        for front in (-1,1):
            box('Pier channel bed',(x,y+front*.167,3.14),(.026,.006,2.88),dark,0)
            box('Pier conductor',(x,y+front*.172,3.14),(.010,.004,2.66),gold,0)
        for z,size,height in ((4.98,.36,.12),(5.10,.43,.12),(5.23,.49,.14)):
            box('Stepped capital',(x,y,z),(size,size,height),stone,.010)
    box('Longitudinal entablature',(x,0,5.30),(.43,7.06,.24),stone,.012)
    box('Entablature shadow course',(x,0,5.46),(.39,7.02,.055),grout,.004)
for y in (-3.325,3.325):
    points=[arch(-math.pi/2+i*math.pi/48,y-.19) for i in range(49)]
    path('Continuous arch backing',points,.36,.38,grout,.005)
    for i in range(24):
        a=-math.pi/2+i*math.pi/24+.002;b=-math.pi/2+(i+1)*math.pi/24-.002
        for front in (-1,1):
            face=y-.215 if front<0 else y+.195
            path('Cut arch voussoir',[arch(a+(b-a)*j/3,face) for j in range(4)],.30,.020,stone,.003)
    for front in (-1,1):
        face=y-.23 if front<0 else y+.215
        path('Raised archivolt moulding',[arch(-math.pi/2+i*math.pi/48,face,.17) for i in range(49)],.07,.025,stone,.004)
    box('Crown keystone',(0,y,6.95),(.28,.49,.45),stone,.012)
frame=module('SM_Aurelion_KIT_AtriumCanopyFrame')
frame.hide_render=True
def vault_patch(name,a,b,y0,y1,low,high,mat=stone,bevel=.003):
    # Closed curved prism with sampled inner/outer surfaces.
    count=max(2,round((b-a)/math.pi*48));angles=[a+(b-a)*i/count for i in range(count+1)]
    points=[arch(t,y,z) for z in (low,high) for y in (y0,y1) for t in angles];n=len(angles);faces=[]
    for i in range(n-1):
        faces += [(i,i+1,n+i+1,n+i),(2*n+i,3*n+i,3*n+i+1,2*n+i+1),(i,2*n+i,2*n+i+1,i+1),(n+i,n+i+1,3*n+i+1,3*n+i)]
    faces += [(0,n,3*n,2*n),(n-1,3*n-1,4*n-1,2*n-1)]
    mesh=bpy.data.meshes.new(name);mesh.from_pydata(points,[],faces);mesh.update()
    for i in range(n-1):
        mesh.polygons[i*4].use_smooth=True;mesh.polygons[i*4+1].use_smooth=True
    # Thin terminal ribs collapse under bevel near the vertical arch spring.
    o=bpy.data.objects.new(name,mesh);scene.collection.objects.link(o);return finish(o,mat,0)
cut=math.asin(.65/3.13)
for a,b in ((-math.pi/2,-cut),(cut,math.pi/2)):
    vault_patch('Continuous weathering shell',a,b,-3.325,3.325,.065,.24,stone,.005)
    for i in range(6):
        t0=a+(b-a)*i/6;t1=a+(b-a)*(i+1)/6
        for j in range(4):
            y0=-3.325+j*1.6625;y1=y0+1.6625
            vault_patch('Recessed coffer field',t0+.008,t1-.008,y0+.065,y1-.065,.035,.065,stone,.003)
            for y in (y0+.008,y1-.044):vault_patch('Coffer transverse moulding',t0,t1,y,y+.036,0,.04,stone,.003)
        for t in (t0+.004,t1-.016):vault_patch('Coffer longitudinal rib',t,t+.012,-3.325,3.325,0,.04,stone,.003)
    theta=b if b<0 else a
    x=3.13*math.sin(theta);z=5.2+1.7*math.cos(theta)
    box('Oculus dressed edge',(x,0,z+.15),(.10,6.65,.30),stone,.007)
    box('Oculus narrow conductor',(x,0,z+.305),(.018,6.61,.010),gold,.001)
roof=module('SM_Aurelion_KIT_AtriumCanopyVault')
frame.hide_render=False
scene.world=bpy.data.worlds.new('Atrium portico studio');scene.world.color=(.16,.16,.16)
for pos,energy in (((6,-8,10),1800),((-6,-3,7),1200),((0,5,10),1600)):
    d=bpy.data.lights.new('Portico softbox','AREA');d.energy=energy;d.size=7;o=bpy.data.objects.new('Portico softbox',d);scene.collection.objects.link(o);o.location=pos;o.rotation_euler=(Vector((0,0,3))-o.location).to_track_quat('-Z','Y').to_euler()
d=bpy.data.cameras.new('Portico review');camera=bpy.data.objects.new('Portico review',d);scene.collection.objects.link(camera);scene.camera=camera;camera.location=(11,-14,6);camera.rotation_euler=(Vector((0,0,3.5))-camera.location).to_track_quat('-Z','Y').to_euler();d.lens=42
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.resolution_x=1400;scene.render.resolution_y=1100;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'Canopy.png')
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Custom split-vault candidate; in-engine fit and lighting review required',modules=manifest),indent=2))
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion_AtriumCanopyKit.blend'));bpy.ops.render.render(write_still=True)
