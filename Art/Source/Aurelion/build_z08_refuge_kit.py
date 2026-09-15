"""Detailed double-faced refuge modules with unscaled 1 m and 2 m variants."""
from pathlib import Path
import bpy,json,math
from mathutils import Vector
base=Path(__file__).resolve().parent
exec(compile((base/'build_architecture_kit.py').read_text().split('# Four metre bay:')[0],'kit_helpers','exec'),globals())
ROOT=base/'Z08RefugeKit';ROOT.mkdir(exist_ok=True)
base_box=box
def box(name,loc,size,mat=stone,bevel=.008):return base_box(name,loc,size,mat,min(bevel,min(size)*.2))
lens=material('M_Aurelion_UplightLens',(.45,.65,.68),.1,.25)
for width in (2.,1.):
    name='SM_Aurelion_KIT_RefugePanel_'+str(int(width))+'m'
    box('Continuous structural core',(0,0,1.53),(width-.12,.22,2.66),dark,.006)
    for z,h,w,d in [(.075,.15,width,.4),(.185,.065,width-.04,.36),(2.855,.06,width-.04,.36),(2.94,.12,width,.4)]:
        box('Cut stone plinth' if z<1 else 'Capped entablature',(0,0,z),(w,d,h),stone,.012)
    for sign in (-1,1):
        box('Solid edge pilaster',(sign*(width/2-.065),0,1.53),(.12,.36,2.6),stone,.012)
        for z in (.32,2.73):box('Pier collar',(sign*(width/2-.065),0,z),(.13,.38,.095),stone,.01)
    for face in (-1,1):
        columns=2 if width==2 else 1;available=width-.48;tile_w=(available-.022*(columns-1))/columns
        for column in range(columns):
            x=(column-(columns-1)/2)*(tile_w+.022)
            for index,z in enumerate((.91,1.56,2.21)):
                box('Individually cut ceramic slab',(x,face*.139,z),(tile_w,.05,.625),stone,.016)
                # Fine isolated corner registration studs and access fasteners.
                for sx in (-1,1):
                    for zz in (z-.26,z+.26):
                        box('Recessed pin socket',(x+sx*(tile_w/2-.045),face*.167,zz),(.026,.009,.026),dark,.003)
                        box('Flush retaining pin',(x+sx*(tile_w/2-.045),face*.174,zz),(.009,.004,.009),gold,.001)
        for sign in (-1,1):
            x=sign*(width/2-.185)
            box('Service channel backing',(x,face*.137,1.52),(.07,.045,2.37),dark,.004)
            box('Functional channel conductor',(x,face*.164,1.5),(.013,.006,2.22),gold,.001)
            for z in (.56,1.43,2.47):
                box('Channel bridge socket',(x,face*.176,z),(.063,.014,.055),dark,.003)
                box('Channel indexing light',(x,face*.187,z),(.031,.006,.014),lens,.001)
        # Open ventilation register with individual blades, separate from solid stone slabs.
        box('Vent register inset',(0,face*.131,.39),(width-.52,.024,.19),dark,.004)
        for z in (.335,.373,.411,.449):
            box('Ventilation blade',(0,face*.158,z),(width-.58,.022,.014),stone,.002)
        # Chamfered shoulder outline gives the upper course a distinct silhouette in relief.
        pts=[(-width/2+.25,face*.174,2.59),(-width/2+.34,face*.174,2.70),(width/2-.34,face*.174,2.70),(width/2-.25,face*.174,2.59)]
        path('Oblique shoulder moulding',pts,.032,.013,stone,.003)
        box('Upper service register',(0,face*.161,2.77),(.24,.028,.065),dark,.003)
        for x in (-.08,-.04,0,.04,.08):box('Register light aperture',(x,face*.179,2.77),(.016,.005,.02),lens,.001)
    o=export(name,[width,.4,3])
    bpy.ops.mesh.primitive_cube_add(size=1,location=(0,0,1.5));hull=bpy.context.object;hull.name='UCX_'+name+'_00';hull.dimensions=(width,.4,3)
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);hull.hide_render=True;hull.display_type='WIRE'
    bpy.ops.object.select_all(action='DESELECT');o.select_set(True);hull.select_set(True);bpy.context.view_layer.objects.active=o
    bpy.ops.export_scene.fbx(filepath=str(ROOT/(name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',apply_unit_scale=True,bake_anim=False,add_leaf_bones=False,mesh_smooth_type='FACE')
    manifest[-1].update(convex_hulls=1,solid_bounds_m=[[-width/2,-.2,0],[width/2,.2,3]],collision='One exact-envelope UCX box; placement collision policy preserved',position_precision=10,preserve_fallback_geometry=True)
    o.location.x=0 if width==2 else 2.0;hull.location.x=o.location.x
(ROOT/'manifest.json').write_text(json.dumps(dict(status='Source authored; engine replacement pending',modules=manifest),indent=2))
scene.world=bpy.data.worlds.new('Refuge studio');scene.world.color=(.15,.15,.15)
for pos,power,size in [((1,-4,5),1500,4),((-3,-2,2),950,3),((1,3,4),1600,3)]:
    bpy.ops.object.light_add(type='AREA',location=pos);a=bpy.context.object;a.data.energy=power;a.data.size=size;a.rotation_euler=(Vector((.6,0,1.5))-a.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(4,-7,3.7));camera=bpy.context.object;camera.rotation_euler=(Vector((.65,0,1.5))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=4.8;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=48;scene.cycles.use_denoising=True;scene.render.resolution_x=1600;scene.render.resolution_y=1200;scene.render.resolution_percentage=100;scene.render.filepath=str(ROOT/'refuge-panels.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Aurelion-Refuge-Panels.blend'));bpy.ops.render.render(write_still=True)
print('REFUGE_KIT_SOURCE_PASS')
