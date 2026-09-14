"""Exterior fit, collision and central passage clearance controls."""
import json
from pathlib import Path
import unreal

def placements(fit):
    x,y,z=fit['world_origin_cm'];outer=fit['outer_x_m'];inner=fit['inner_x_m'];rows=[]
    for end,sign,yaw in [('South',-1,180),('North',1,0)]:
        for side,s in [('West',-1),('East',1)]:
            for i in range(2):rows.append((f'{end}_{side}_{i}','Z02Exterior_4m',(x+s*(outer-2-4*i)*100,y+sign*fit['roof_y_half_m']*100,fit['wall_base_z_cm']),yaw))
            rows.append((f'{end}_{side}_Closing','Z02Exterior_Closing',(x+s*(inner+(outer-inner-8)/2)*100,y+sign*fit['roof_y_half_m']*100,fit['wall_base_z_cm']),yaw))
    rows.append(('Roof','Z02RoofFinish',(x,y,fit['roof_base_z_cm']),0));return rows

def hit(raw):
    return next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw

def check_exterior(world,actors):
    fit=json.loads((Path(unreal.Paths.project_dir())/'Art/Source/Aurelion/Z02ExteriorKit/exterior-fit.json').read_text());labels={a.get_actor_label():a for a in actors};sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);checked=[];surfaces=[]
    for name,suffix,pos,yaw in placements(fit):
        a=labels['KIT_Z02_Exterior_'+name];c=a.static_mesh_component;p=a.get_actor_location();s=a.get_actor_scale3d()
        assert max(abs(v-w) for v,w in zip((p.x,p.y,p.z),pos))<.01 and all(abs(v-1)<.001 for v in (s.x,s.y,s.z))
        assert abs((a.get_actor_rotation().yaw-yaw+180)%360-180)<.01 and c.static_mesh.get_name()=='SM_Aurelion_KIT_'+suffix
        assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game') and a.get_actor_enable_collision()
        assert c.get_collision_enabled()==unreal.CollisionEnabled.QUERY_AND_PHYSICS and str(c.get_collision_profile_name())=='BlockAll'
        assert sm.get_convex_collision_count(c.static_mesh)==1 and sm.get_simple_collision_count(c.static_mesh)==0
        assert sm.get_num_uv_channels(c.static_mesh,0)==2 and sm.get_nanite_settings(c.static_mesh).get_editor_property('enabled')
        ignored=[other for other in actors if other!=a]
        if name!='Roof':
            for z in (100,400,850):
                raw=unreal.SystemLibrary.line_trace_single(world,unreal.Vector(p.x,p.y-100,p.z+z),unreal.Vector(p.x,p.y+100,p.z+z),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,ignored,unreal.DrawDebugTrace.NONE,True)
                h=hit(raw);assert h and h.to_tuple()[0],(name,z);surfaces.append(dict(name=name,z=z))
        else:
            settings=sm.get_nanite_settings(c.static_mesh)
            assert settings.get_editor_property('fallback_target')==unreal.NaniteFallbackTarget.PERCENT_TRIANGLES
            assert settings.get_editor_property('fallback_percent_triangles')==1.0 and settings.get_editor_property('fallback_relative_error')==0.0
            for dx in (-1000,0,1000):
                for dy in (-600,0,600):
                    for complex_trace in (False,True):
                        h=hit(unreal.SystemLibrary.line_trace_single(world,unreal.Vector(p.x+dx,p.y+dy,p.z+100),unreal.Vector(p.x+dx,p.y+dy,p.z-20),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,complex_trace,ignored,unreal.DrawDebugTrace.NONE,True))
                        assert h and h.to_tuple()[0] and abs(h.to_tuple()[5].z-(p.z+12))<.3,(dx,dy,complex_trace,h.to_tuple()[5].z if h and h.to_tuple()[0] else None,p.z+12)
                        surfaces.append(dict(name=name,x=dx,y=dy,complex=complex_trace))
        checked.append(a)
    ignored=[a for a in actors if a not in checked];capsules=[];x,y,z=fit['world_origin_cm']
    for sign in (-1,1):
        end_y=y+sign*fit['roof_y_half_m']*100
        for dx in (-550,0,550,-800,800):
            h=hit(unreal.SystemLibrary.capsule_trace_single_by_profile(world,unreal.Vector(x+dx,end_y-150,200),unreal.Vector(x+dx,end_y+150,200),42,88,'Pawn',False,ignored,unreal.DrawDebugTrace.NONE,True))
            expected=abs(dx)==800;assert bool(h and h.to_tuple()[0])==expected,(sign,dx)
            capsules.append(dict(end=sign,x=dx,blocking=expected))
    return dict(placements=len(checked),surface_controls=surfaces,pawn_controls=capsules,qualification='Isolated exterior blocking/clearance and roof-height checks; rooftop use, live route/navigation, lighting and final art unqualified.')
