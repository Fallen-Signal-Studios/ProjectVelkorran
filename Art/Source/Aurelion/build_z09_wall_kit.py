"""Wound gallery: fitted two-sided masonry, baffles and doorway heads."""
from pathlib import Path
helper=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helper.read_text().split('# Four metre bay:')[0],str(helper),'exec'))
ROOT=Path(__file__).resolve().parent/'Z09WallKit';ROOT.mkdir(exist_ok=True)
baseline=json.loads((ROOT/'wall-baseline.json').read_text(encoding='utf-8-sig'))

def wall(name,w,d,h):
    box('Continuous stone core',(0,0,h/2),(w-.012,d*.45,h-.008),stone,.004)
    courses=4 if h>3 else 2
    for z,height,depth in ((.06,.12,d),(.18,.10,d*.88),(h-.23,.10,d*.86),(h-.09,.18,d)):
        box('Dressed perimeter course',(0,0,z),(w-.004,depth,height),stone,.006)
    for face in (-1,1):
        for row in range(courses):
            z0=.28+row*(h-.65)/courses;z1=.28+(row+1)*(h-.65)/courses
            for side in (-1,1):
                x=side*w/4
                box('Dressed ashlar',(x,face*d*.29,(z0+z1)/2),(w/2-.025,d*.18,z1-z0-.016),stone,.008)
        for side in (-1,1):
            x=side*(w/2-.105)
            box('Recessed edge joint',(x,face*d*.395,h/2),(.14,.015,h-.58),dark,.002)
            box('Chamfered edge pilaster',(x,face*d*.435,h/2),(.07,.04,h-.59),stone,.004)
            if h>3:
                px=side*w/4;pw=w/2-.43
                box('Recessed panel bed',(px,face*d*.397,h*.48),(pw,.016,h*.54),dark,.003)
                box('Carved stone field',(px,face*d*.428,h*.48),(pw-.06,.028,h*.54-.065),stone,.008)
                for sx in (-1,1):
                    box('Fine incised panel border',(px+sx*(pw/2-.09),face*d*.49,h*.48),(.014,.008,h*.54-.19),dark,.001)
                points=[(side*(w/2-.29),face*d*.46,.36),(side*(w/2-.29),face*d*.46,h*.74),
                        (side*(w/2-.68),face*d*.46,h*.84),(side*(w/2-.68),face*d*.46,h-.37)]
                # Path extrusion follows +Y: offset the rear face to retain a symmetric envelope.
                path('Recessed oblique conductor',[(x,y-(.014 if face<0 else 0),z) for x,y,z in points],.024,.014,gold,.002)
                for j in range(5):box('Lower service register',(side*(w/2-.45),face*d*.475,.40+j*.045),(.18,.012,.012),gold,.001)
            else:
                box('Lintel relief field',(side*w/4,face*d*.425,h*.49),(w/2-.28,.028,h*.42),stone,.006)
                box('Lintel fine conductor',(side*w/4,face*d*.485,h*.73),(w/2-.34,.012,.018),gold,.002)
    obj=export(name,[w,d,h]);obj.hide_render=True
    manifest[-1]['nominal_dimensions_m']=list(obj.dimensions)
    manifest[-1].update(position_precision=10,preserve_fallback_geometry=True,collision='None; existing wound-gallery obstruction remains authoritative')
    return obj

specs=[('Z09WallSide',55/14,.25,6),('Z09WallEnd',5,.25,6),('Z09Baffle',5,.20,5),('Z09Lintel',6,.25,1.5)]
objects={name:wall('SM_Aurelion_KIT_'+name,w,d,h) for name,w,d,h in specs}
placements=[]
for row in baseline['instances']:
    sx,sy,sz=row['scale'];w,h=4*sx,4*sz
    suffix='Z09Lintel' if h<2 else 'Z09Baffle' if h<5.5 else 'Z09WallSide' if w<4.5 else 'Z09WallEnd'
    q=row['quaternion'];assert abs(q[0])+abs(q[1])<1e-5
    yaw=2*math.atan2(q[2],q[3]);x,y,z=row['location']
    ox=baseline['mesh_origin'][0]*sx;oy=baseline['mesh_origin'][1]*sy
    placements.append(dict(asset='SM_Aurelion_KIT_'+suffix,source_index=row['index'],location_cm=[x+math.cos(yaw)*ox-math.sin(yaw)*oy,y+math.sin(yaw)*ox+math.cos(yaw)*oy,z],yaw=math.degrees(yaw)))
(ROOT/'manifest.json').write_text(json.dumps(dict(modules=manifest,placements=placements),indent=2))
objects['Z09WallSide'].hide_render=False;objects['Z09Baffle'].hide_render=False;objects['Z09Baffle'].location.x=5.1
scene.world=bpy.data.worlds.new('Wound gallery masonry studio');scene.world.color=(.17,.17,.17)
target=Vector((2.5,0,2.7))
for pos,power,size in [((1,-7,8),2800,5),((-6,-2,5),1700,5),((8,3,8),2300,4)]:
    bpy.ops.object.light_add(type='AREA',location=pos);o=bpy.context.object;o.data.energy=power;o.data.size=size;o.rotation_euler=(target-o.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(12,-19,10));o=bpy.context.object;o.rotation_euler=(target-o.location).to_track_quat('-Z','Y').to_euler();o.data.type='ORTHO';o.data.ortho_scale=13;scene.camera=o
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.resolution_x=1600;scene.render.resolution_y=1100;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'wall-kit.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Z09-Walls.blend'));bpy.ops.render.render(write_still=True)
