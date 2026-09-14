"""End-wall geometry and gate authority checks, without applying journal state."""
import json
from pathlib import Path
import unreal

def gate_authority(a):
    return dict(transform=a.get_actor_transform().export_text(),body_transform=a.body.get_world_transform().export_text(),
        body_extent=a.body.get_unscaled_box_extent().export_text(),body_collision=str(a.body.get_collision_enabled()),
        contract={k:str(a.get_editor_property(k)) for k in ('mission_id','beat_id','block_after_completion','use_gate_body')},
        bound_visual_actors=[v.get_actor_label() for v in a.get_editor_property('bound_visual_actors')])

def check_endwalls(world,actors):
    labels={a.get_actor_label():a for a in actors}
    baseline=json.loads((Path(unreal.Paths.project_dir())/'Art/Source/Aurelion/EndwallKit/gate-baseline.json').read_text())
    for row in baseline[:2]:
        current=gate_authority(labels[row['label']]); body=row['components'][0]
        assert current==dict(transform=row['transform'],body_transform=body['transform'],body_extent=body['unscaled_extent'],
            body_collision=body['collision'],contract=row['contract'],bound_visual_actors=row['bound_visual_actors']),current
    def hit(raw):
        if raw is None:return None
        h=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
        return h if h.to_tuple()[0] else None
    physical=[a for a in actors if a.get_actor_label().startswith(('Z01_Wall','Z01_Lintel')) or a.get_actor_label()=='aureliondoors']
    assert len(physical)>=7
    ignored=[a for a in actors if a not in physical]
    clear=[]; controls=[]; upper=[]
    for side,y,gate in (('South',-17500,'Aurelion_TarrikArrivalGate'),('North',-11900,'Aurelion_PressureHallExit')):
        for x in (-7252,-7000,-6748):
            for z in (93,225,326,356):
                raw=unreal.SystemLibrary.capsule_trace_single_by_profile(world,unreal.Vector(x,y-300,z),unreal.Vector(x,y+300,z),42,88,'Pawn',False,ignored,unreal.DrawDebugTrace.NONE,True)
                if z==356:
                    h=hit(raw)
                    assert h and h.to_tuple()[9]==labels['aureliondoors'], 'Unexpected upper boundary; remeasure soffit fit'
                    upper.append(dict(side=side,x=x,z=z,blocked=True,actor=h.to_tuple()[9].get_actor_label(),impact=h.to_tuple()[5].export_text()))
                    continue
                assert not hit(raw),(side,x,z,'Retained passage blocked',str(hit(raw).to_tuple()) if hit(raw) else None)
                clear.append(dict(side=side,x=x,z=z))
        for x,z in ((-7350,225),(-6650,225),(-7000,510)):
            raw=unreal.SystemLibrary.line_trace_single(world,unreal.Vector(x,y-300,z),unreal.Vector(x,y+300,z),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,ignored,unreal.DrawDebugTrace.NONE,True)
            assert hit(raw),(side,x,z,'Wall/lintel control did not block')
            controls.append(dict(side=side,x=x,z=z))
        gate_ignored=[a for a in actors if a!=labels[gate]]
        raw=unreal.SystemLibrary.capsule_trace_single_by_profile(world,unreal.Vector(-7000,y-400,93),unreal.Vector(-7000,y+400,93),42,88,'Pawn',False,gate_ignored,unreal.DrawDebugTrace.NONE,True)
        assert hit(raw),(gate,'Closed gate body did not block')
    return dict(clear_aperture_capsules=clear,upper_aperture_audit=upper,blocked_wall_controls=controls,closed_gate_controls=2,
        qualification='Isolated editor geometry, with gate actors excluded for aperture probes. No journal changes or live gameplay qualification.')
