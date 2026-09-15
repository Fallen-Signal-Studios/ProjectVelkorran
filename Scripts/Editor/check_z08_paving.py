"""Measured paving fit and retained Z08 floor collision, excluding gameplay claims."""
import json,runpy
from pathlib import Path
import unreal

def check_z08_paving(world, actors):
    labels={a.get_actor_label():a for a in actors}
    fit=json.loads((Path(unreal.Paths.project_dir())/'Art/Source/Aurelion/Z08PavingKit/floor-fit.json').read_text())
    court_fit=json.loads((Path(unreal.Paths.project_dir())/'Art/Source/Aurelion/Z08CourtKit/court-fit.json').read_text()) if 'KIT_Z08_IsolationCourt' in labels else None
    hidden_underlay={r['actor'] for r in court_fit['underlay']} if court_fit else set()
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    placements=[];asset_collision={}
    for ci,col in enumerate(fit['columns']):
        for ri,row in enumerate(fit['rows']):
            y=row['y']
            suffix=col['suffix']
            a=labels[f'KIT_Z08_Paving_{ci:02}_{ri:02}'];c=a.static_mesh_component;m=c.static_mesh
            p=a.get_actor_location();s=a.get_actor_scale3d();o,e=a.get_actor_bounds(False)
            assert abs(p.x-col['x'])<.01 and abs(p.y-y)<.01 and abs(o.z+e.z+1200)<.01
            assert abs(e.x*2-col['width']*100)<.01 and abs(e.y*2-row['length']*100)<.01
            r=a.get_actor_rotation()
            assert max(abs(v-1) for v in (s.x,s.y,s.z))<.001 and max(abs(v) for v in (r.pitch,r.yaw,r.roll))<.001
            assert m.get_name()=='SM_Aurelion_KIT_'+suffix
            hidden=a.get_actor_label() in hidden_underlay
            assert c.get_editor_property('visible')== (not hidden) and c.get_editor_property('hidden_in_game')==hidden
            assert not a.get_actor_enable_collision() and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
            assert sm.get_num_uv_channels(m,0)==2 and sm.get_nanite_settings(m).get_editor_property('enabled')
            # These shared, previously imported paving assets may retain import-generated hulls.
            # Deployment disables both actor and component collision; the retained room floor is authoritative.
            asset_collision[m.get_name()]=dict(simple=sm.get_simple_collision_count(m),convex=sm.get_convex_collision_count(m))
            placements.append(a.get_actor_label())
    c=labels[fit['floor']['actor']].get_component_by_class(unreal.InstancedStaticMeshComponent)
    expected=fit['retained_transforms']
    if c.static_mesh.get_name()=='SM_Aurelion_KIT_Z08DeckAssembly':
        source=Path(unreal.Paths.project_dir())/'Art/Source/Aurelion/Z08DeckKit'
        original=json.loads((source/'deck-baseline.json').read_text())
        assert sorted(row['transform'] for row in original['instances'])==sorted(expected)
        runpy.run_path(str(Path(unreal.Paths.project_dir())/'Scripts/Editor/check_z08_decking.py'))['check_z08_decking'](actors)
    else:
        assert sorted(c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count()))==sorted(expected)
    retained_floor_count=c.get_instance_count()
    center=fit['centerline'];c=labels[center['actor']].get_component_by_class(unreal.InstancedStaticMeshComponent)
    # HISM removal swaps retained slots; require the exact transform multiset.
    retained_centerline=court_fit['retained_transforms'] if court_fit else fit['retained_centerline']
    threshold=labels['Z08_WestSurvivorRecess_ThresholdMark'].get_component_by_class(unreal.StaticMeshComponent)
    if threshold.static_mesh.get_name()=='SM_Aurelion_KIT_RefugeThreshold_6m':
        source=Path(unreal.Paths.project_dir())/'Art/Source/Aurelion/RefugeShellKit'
        overlay=json.loads((source/'threshold-overlay-baseline.json').read_text())
        assert overlay['actor']==center['actor'] and sorted(overlay['all_transforms'])==sorted(retained_centerline)
        # Three formerly retained bands are now authored embedded inlays. Require
        # their replacements and preserve the other two exact guidance poses.
        runpy.run_path(str(Path(unreal.Paths.project_dir())/'Scripts/Editor/check_refuge_shell.py'))['check_refuge_shell'](actors)
        retained_centerline=overlay['all_transforms'][2:4]
    assert sorted(c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count()))==sorted(retained_centerline)
    assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
    for row in fit['guides']:
        a=labels[row['actor']];c=a.static_mesh_component
        assert a.get_actor_transform().export_text()==row['actor_transform'] and c.static_mesh.get_path_name()==row['mesh']
        assert not c.get_editor_property('visible') and c.get_editor_property('hidden_in_game')
        assert not a.get_actor_enable_collision() and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
    floor=labels['Z08_Floor'];c=floor.static_mesh_component
    assert floor.get_actor_transform().export_text()==fit['physical_floor']['actor_transform']
    assert c.static_mesh.get_path_name()==fit['physical_floor']['mesh'] and floor.get_actor_enable_collision()
    assert c.get_collision_enabled()==unreal.CollisionEnabled.QUERY_AND_PHYSICS
    ignored=[a for a in actors if a not in [floor]+[labels[r['actor']] for r in fit['guides']]];probes=[]
    for x in [c['x'] for c in fit['columns']]+[-180,180]:
        for row in fit['rows']:
            y=row['y']
            raw=unreal.SystemLibrary.line_trace_single(world,unreal.Vector(x,y,-1000),unreal.Vector(x,y,-1300),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,ignored,unreal.DrawDebugTrace.NONE,True)
            hit=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
            assert hit and hit.to_tuple()[0],(x,y)
            t=hit.to_tuple();assert t[9]==floor and abs(t[5].z+1200)<.01,(x,y,t[5])
            probes.append(dict(x=x,y=y,z=t[5].z))
    return dict(placements=len(placements),hidden_court_underlay=len(hidden_underlay),shared_asset_collision=asset_collision,removed_floor_instances=216,retained_floor_instances=retained_floor_count,retained_centerline_instances=len(retained_centerline),floor_queries=probes,guide_collision_change='Two 6cm-raised decorative guides retired; floor now consistently Z -1200.',qualification='Stopped-editor fit and isolated collision checks; live traversal, navigation, combat and performance remain unqualified.')
