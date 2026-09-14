"""Measured furniture placement, surface height and simple blocking checks."""
import json,math,re
from pathlib import Path
import unreal
def placements(fit):
    rows=[]
    for i,b in enumerate(fit['baseline']):
        parse=lambda key:[float(v) for v in re.findall('[XYZW]=([-+0-9.]+)',re.search(key+r'=\(([^)]+)\)',b['transform']).group(1))]
        p=parse('Translation');q=parse('Rotation');yaw=2*math.atan2(q[2],q[3]);co,si=math.cos(yaw),math.sin(yaw)
        for item in fit['layout']:
            x,y=item['x']*100,-item['y']*100
            rows.append((f'{i}_{item["name"]}',item['mesh'],(p[0]+co*x-si*y,p[1]+si*x+co*y,fit['floor_z']),math.degrees(yaw)))
    return rows
def check_furniture(world,actors):
    source=Path(unreal.Paths.project_dir())/'Art/Source/Aurelion/Z02FurnitureKit';fit=json.loads((source/'furniture-fit.json').read_text(encoding='utf-8-sig'));specs={s['asset']:s for s in json.loads((source/'manifest.json').read_text())['modules']}
    labels={a.get_actor_label():a for a in actors};sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);checked=[];probes=[]
    for name,suffix,pos,yaw in placements(fit):
        a=labels['KIT_Z02_Furniture_'+name];c=a.static_mesh_component;p=a.get_actor_location();scale=a.get_actor_scale3d()
        assert max(abs(v-w) for v,w in zip((p.x,p.y,p.z),pos))<.01
        assert all(abs(v-1)<.001 for v in (scale.x,scale.y,scale.z)) and abs((a.get_actor_rotation().yaw-yaw+180)%360-180)<.01
        assert c.static_mesh.get_name()=='SM_Aurelion_KIT_'+suffix and c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
        assert a.get_actor_enable_collision() and c.get_collision_enabled()==unreal.CollisionEnabled.QUERY_AND_PHYSICS
        assert sm.get_convex_collision_count(c.static_mesh)==3 and sm.get_simple_collision_count(c.static_mesh)==0
        assert sm.get_num_uv_channels(c.static_mesh,0)==2 and sm.get_nanite_settings(c.static_mesh).get_editor_property('enabled')
        ignored=[other for other in actors if other!=a];t=a.get_actor_transform()
        for x,y,z in specs[c.static_mesh.get_name()]['furniture_surface_samples']:
            start=unreal.MathLibrary.transform_location(t,unreal.Vector(x*100,-y*100,300));end=unreal.MathLibrary.transform_location(t,unreal.Vector(x*100,-y*100,-100))
            raw=unreal.SystemLibrary.line_trace_single(world,start,end,unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,ignored,unreal.DrawDebugTrace.NONE,True)
            hit=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw;blocking=bool(hit and hit.to_tuple()[0])
            assert blocking==(z is not None),(name,x,y,z)
            if blocking:assert abs(hit.to_tuple()[5].z-(pos[2]+z*100))<.2
            probes.append(dict(actor=name,x=x,y=y,height=z))
        for x,expected in ((0,True),(150,False)):
            start=unreal.MathLibrary.transform_location(t,unreal.Vector(x,-150,93));end=unreal.MathLibrary.transform_location(t,unreal.Vector(x,150,93))
            raw=unreal.SystemLibrary.capsule_trace_single_by_profile(world,start,end,42,88,'Pawn',False,ignored,unreal.DrawDebugTrace.NONE,True)
            hit=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
            assert bool(hit and hit.to_tuple()[0])==expected,(name,x,expected)
        checked.append(name)
    for b in fit['baseline']:
        a=labels[b['actor']];c=a.static_mesh_component
        assert a.get_actor_transform().export_text()==b['transform'] and c.static_mesh.get_path_name()==b['mesh']
        assert not a.get_actor_enable_collision() and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and str(c.get_collision_profile_name())=='NoCollision'
        assert not c.get_editor_property('visible') and c.get_editor_property('hidden_in_game')
    return dict(placements=checked,surface_probes=probes,pawn_capsule_controls=len(checked)*2,qualification='Isolated authored collision and placement checks; full-room movement, navigation and final art remain unqualified.')
