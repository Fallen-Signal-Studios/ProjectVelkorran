"""Measured Z02 paving grid and retained floor collision checks."""
import json
from pathlib import Path
import unreal

def check_paving(world,actors):
    labels={a.get_actor_label():a for a in actors}; root=Path(unreal.Paths.project_dir())
    fit=json.loads((root/'Art/Source/Aurelion/Z02PavingKit/floor-fit.json').read_text()); checked=[]
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    for ci,col in enumerate(fit['columns']):
        for ri,row in enumerate(fit['rows']):
            suffix='Z02Paving'+col['kind'].title()+'_Short' if row['short'] else col['suffix']
            a=labels[f'KIT_Z02_Paving_{ci:02}_{ri:02}']; p=a.get_actor_location(); scale=a.get_actor_scale3d(); o,e=a.get_actor_bounds(False)
            assert abs(p.x-col['x'])<.01 and abs(p.y-row['y'])<.01 and abs(o.z+e.z-fit['surface_z'])<.01
            assert all(abs(v-1)<.001 for v in (scale.x,scale.y,scale.z)) and abs(a.get_actor_rotation().yaw)<.01
            assert abs(e.x*2-col['width']*100)<.01 and abs(e.y*2-row['length']*100)<.01
            c=a.static_mesh_component; mesh=c.static_mesh
            assert mesh.get_name()=='SM_Aurelion_KIT_'+suffix
            assert not a.get_actor_enable_collision() and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
            assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
            assert sm.get_num_uv_channels(mesh,0)==2 and sm.get_nanite_settings(mesh).get_editor_property('enabled')
            checked.append(a.get_actor_label())
    retained=[labels[name] for name in ('Cube2','Z02_Floor','Z02__GoldChannel_01','Z02__GoldChannel_02')]
    ignored=[a for a in actors if a not in retained]; probes=[]
    for x in [c['x'] for c in fit['columns']]+[-7180,-6820]:
        for y in [r['y'] for r in fit['rows']]:
            raw=unreal.SystemLibrary.line_trace_single(world,unreal.Vector(x,y,100),unreal.Vector(x,y,-50),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,ignored,unreal.DrawDebugTrace.NONE,True)
            hit=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
            assert hit and hit.to_tuple()[0]
            z=hit.to_tuple()[5].z; assert abs(z-fit['surface_z'])<.15,(x,y,z,fit['surface_z'])
            probes.append(dict(x=x,y=y,z=z))
    for label in ('Cube2','Z02__GoldChannel_01','Z02__GoldChannel_02'):
        c=labels[label].static_mesh_component
        assert not c.get_editor_property('visible') and c.get_editor_property('hidden_in_game')
        assert c.get_collision_enabled()==unreal.CollisionEnabled.QUERY_AND_PHYSICS
    legacy=labels['Aurelion_Art_M12_Z02_11_859d0a'].get_component_by_class(unreal.InstancedStaticMeshComponent)
    assert legacy.get_instance_count()==14 and not legacy.get_editor_property('visible') and legacy.get_editor_property('hidden_in_game')
    return dict(placements=checked,floor_queries=probes,surface_z=fit['surface_z'],qualification='Isolated retained floor queries; interactions, transition movement, navigation and performance are not qualified.')
