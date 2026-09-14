"""Fitted Crucible guardrail, with paired legacy instances consolidated."""
from pathlib import Path
from mathutils import Quaternion
helper=Path(__file__).with_name('build_architecture_kit.py')
exec(compile(helper.read_text().split('# Four metre bay:')[0],str(helper),'exec'))
ROOT=Path(__file__).resolve().parent/'Z08RailingKit';ROOT.mkdir(exist_ok=True)
baseline=json.loads((ROOT/'rail-baseline.json').read_text());origin=baseline['mesh_origin'];extent=baseline['mesh_extent']
w,d,h=[e/50 for e in extent];cx,cy,cz=[origin[0]/100,-origin[1]/100,(origin[2]-extent[2])/100]
# Three load-bearing dressed-stone uprights, open dark infill, thin gold registers.
for x in (-w/2+.07,0,w/2-.07):
    for z,pw,pd,ph in [(.025,.14,d,.05),(.068,.115,d*.86,.036),(h*.49,.078,d*.61,h-.20),(h-.091,.105,d*.83,.045),(h-.051,.13,d,.035)]:
        box('Dressed post course',(x,0,z),(pw,pd,ph),stone,.005)
    for side in (-1,1):
        box('Recessed post flute',(x,side*(d*.305+.001),h*.49),(.035,.006,h-.25),dark,.001)
        box('Fine post conductor',(x,side*(d*.305+.0045),h*.49),(.007,.003,h-.29),gold,.0005)
        for dz in (.075,h-.085):
            box('Mount fastener',(x,side*(d*.43+.002),dz),(.018,.006,.011),gold,.001)
# Continuous dark grip keeps broad gold out of the eye line.
box('Continuous ergonomic handrail',(0,0,h-.018),(w,d*.62,.036),dark,.008)
for side in (-1,1):
    box('Narrow handrail gold register',(0,side*(d*.31+.001),h-.026),(w-.015,.003,.006),gold,.0007)
box('Infill lower tie',(0,0,.16),(w-.16,.035,.028),dark,.004)
for i in range(1,20):
    x=-w/2+i*w/20
    if abs(x)<.08:continue
    box('Slender guard baluster',(x,0,(h+.12)/2),(.018,.025,h-.20),dark,.003)
    for z in (.18,h-.09):box('Baluster ferrule',(x,0,z),(.026,.033,.023),dark,.003)
for obj in parts:obj.location+=Vector((cx,cy,cz))
obj=export('SM_Aurelion_KIT_Z08Guardrail',[w,d,h])
manifest[-1]['nominal_dimensions_m']=list(obj.dimensions)
manifest[-1]['collision']='None; native barriers retained'
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Authored rail; engine fit and visual acceptance pending',modules=manifest),indent=2))
# Adjacent duplicate rails share rotation/scale and touch along local thickness.
used=set();merged=[]
for row in baseline['instances']:
    if row['index'] in used:continue
    q=row['quaternion'];rotation=Quaternion((q[3],q[0],q[1],q[2]));lateral=rotation@Vector((0,1,0));mate=None
    for other in baseline['instances']:
        if other['index']<=row['index'] or other['index'] in used:continue
        if max(abs(a-b) for a,b in zip(q,other['quaternion']))>1e-6 or max(abs(a-b) for a,b in zip(row['scale'],other['scale']))>1e-6:continue
        delta=Vector(other['location'])-Vector(row['location']);along=delta.dot(lateral)
        if (delta-lateral*along).length<.001 and abs(abs(along)-2*extent[1]*row['scale'][1])<.001:mate=other;break
    ids=[row['index']];loc=list(row['location']);scale=list(row['scale'])
    if mate:
        ids.append(mate['index']);used.add(mate['index']);loc=[(a+b)/2 for a,b in zip(loc,mate['location'])];scale[1]*=2
    used.add(row['index']);merged.append(dict(original_indices=ids,location=loc,scale=scale,quaternion=q))
assert len(used)==44 and len(merged)==26
(ROOT/'rail-fit.json').write_text(json.dumps(dict(placements=merged,original_instances=44,fitted_instances=26,paired_runs=18),indent=2))
scene.world=bpy.data.worlds.new('Guardrail studio');scene.world.color=(.18,.18,.18)
for pos,power,size in [((1,-3,3),800,3),((-3,1,2),600,3)]:
    bpy.ops.object.light_add(type='AREA',location=pos);o=bpy.context.object;o.data.energy=power;o.data.size=size;o.rotation_euler=(Vector((0,0,.5))-o.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(4,-7,3));camera=bpy.context.object;camera.rotation_euler=(Vector((0,0,.5))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=4.8;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=48;scene.cycles.use_denoising=True;scene.render.resolution_x=1500;scene.render.resolution_y=900;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'guardrail.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Crucible-Guardrail.blend'));bpy.ops.render.render(write_still=True)
print('CRUCIBLE_GUARDRAIL_BUILD_PASS')
