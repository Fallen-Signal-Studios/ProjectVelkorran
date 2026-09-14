"""Measured perimeter placements; no mission or live movement qualification."""
import json
from pathlib import Path
import unreal

def placements(fit):
    rows=[]; start=fit['center_y']-fit['wall_length_m']*50
    for side,x,yaw in (('West',fit['west_inner_x'],-90),('East',fit['east_inner_x'],90)):
        for i in range(5):
            width=4 if i<4 else fit['wall_length_m']-16
            rows.append((f'{side}_{i}','Z02Perimeter_4m' if i<4 else 'Z02Perimeter_Closing',(x,start+400*i+width*50,fit['wall_base_z']),yaw))
    for end in fit['ends']:rows.append((end['label']+'_Frieze','Z02EndFrieze',(end['x'],end['y'],end['z']),0))
    return rows

def check_perimeter(world,actors):
    root=Path(unreal.Paths.project_dir()); source=root/'Art/Source/Aurelion/Z02PerimeterKit'
    fit=json.loads((source/'perimeter-fit.json').read_text()); labels={a.get_actor_label():a for a in actors}
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem); checked=[]
    for name,suffix,pos,yaw in placements(fit):
        a=labels['KIT_Z02_Perimeter_'+name];c=a.static_mesh_component;p=a.get_actor_location();scale=a.get_actor_scale3d()
        assert max(abs(v-w) for v,w in zip((p.x,p.y,p.z),pos))<.01
        assert all(abs(v-1)<.001 for v in (scale.x,scale.y,scale.z)) and abs(a.get_actor_rotation().yaw-yaw)<.01
        assert c.static_mesh.get_name()=='SM_Aurelion_KIT_'+suffix
        assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
        assert not a.get_actor_enable_collision() and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
        assert sm.get_num_uv_channels(c.static_mesh,0)==2 and sm.get_nanite_settings(c.static_mesh).get_editor_property('enabled')
        assert sm.get_simple_collision_count(c.static_mesh)==sm.get_convex_collision_count(c.static_mesh)==0
        checked.append(a.get_actor_label())
    for baseline in fit['baseline']:
        a=labels[baseline['actor']];c=a.static_mesh_component
        assert a.get_actor_transform().export_text()==baseline['actor_transform']
        assert c.static_mesh.get_path_name()==baseline['mesh']
        assert str(c.get_collision_enabled())==baseline['collision'] and str(c.get_collision_profile_name())==baseline['profile']
        assert not c.get_editor_property('visible') and c.get_editor_property('hidden_in_game')
    # Verify the retained wall still blocks at walking height and the new upper
    # frieze has not introduced a physical obstruction into its lower aperture.
    retained=[labels[k] for k in ('Cube_3','Cube_6','Cube_7')]; ignored=[a for a in actors if a not in retained]; probes=[]
    for side,end_x in (('West',-8600),('East',-5400)):
        for y in (-9600,-9200,-8800,-8400):
            raw=unreal.SystemLibrary.line_trace_single(world,unreal.Vector(-7000,y,150),unreal.Vector(end_x,y,150),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,ignored,unreal.DrawDebugTrace.NONE,True)
            hit=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
            assert hit and hit.to_tuple()[0] and hit.to_tuple()[9]==labels['Cube_3']
            probes.append(dict(side=side,y=y,impact=hit.to_tuple()[5].export_text()))
    clear=[]
    for end in fit['ends']:
        for x in (-7200,-7000,-6800):
            for z in (200,400):
                raw=unreal.SystemLibrary.capsule_trace_single_by_profile(world,unreal.Vector(x,end['y']-150,z),unreal.Vector(x,end['y']+150,z),42,88,'Pawn',False,ignored,unreal.DrawDebugTrace.NONE,True)
                hit=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
                assert not hit or not hit.to_tuple()[0],(end['label'],x,z)
                clear.append(dict(end=end['label'],x=x,z=z))
    return dict(placements=checked,retained_wall_queries=probes,isolated_clear_end_capsules=clear,qualification='Isolated shell queries and authoring preservation only; full passage movement, collision correspondence and performance remain unqualified.')
