"""Verify saved paving, collision, seating, walls and HUD through earned CP9 loading."""
from pathlib import Path
import hashlib,json,runpy
import unreal
root=Path(unreal.Paths.project_dir())
saved=json.loads((root/'Saved/Validation/Aurelion/DeparturePavingSaved-20260921-000452-0b013301/departure-paving-fit.json').read_text())
assert saved['status']=='saved_reloaded'
for asset,digest in saved['mesh_hashes'].items():
    assert hashlib.sha256((root/('Content/'+asset.removeprefix('/Game/')+'.uasset')).read_bytes()).hexdigest()==digest
seats=runpy.run_path(str(Path(__file__).with_name('verify_m13_departure_seats.py')))
driver=seats['driver'];parent=seats['parent'];previous_finish=seats['finish']
driver['views']=[('player',None,None),('floor-overview',(0,47900,450),-90),('paving-detail',(500,48000,170),-90),
    ('dock',(3800,49000,1800),-90),('selene-background',(1350,48200,170),-90)]
driver['DEPARTURE_VIEW_PITCHES']={'floor-overview':-30,'paving-detail':-35,'dock':-50}

def finish(error=None):
    if error:previous_finish(error);return
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    actors=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor);actual={}
    for key,row in saved['expected'].items():
        actor=next(a for a in actors if a.get_actor_label()==row['actor'])
        c=next(c for c in actor.get_components_by_class(unreal.InstancedStaticMeshComponent) if c.get_name()==row['component'])
        assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and not c.get_editor_property('can_ever_affect_navigation')
        assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
        actual[key]=dict(actor=actor.get_actor_label(),component=c.get_name(),mesh=c.static_mesh.get_path_name(),
            transforms=[c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())])
    assert actual==saved['expected']
    for row in saved['native_floor_probes']:
        x,y=row['x'],row['y']
        raw=unreal.SystemLibrary.line_trace_single(world,unreal.Vector(x,y,100),unreal.Vector(x,y,-100),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[],unreal.DrawDebugTrace.NONE,True)
        hit=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw;t=hit.to_tuple() if hit else None
        if row['actor'] is None:assert not t or not t[0],(x,y,'Scenic dock blocks in PIE')
        else:assert t and t[0] and t[9].get_actor_label()==row['actor'] and abs(t[5].z)<.1,(x,y,'Physical floor changed in PIE')
    parent['report']['departure_paving_review']=dict(saved_paving=actual,physical_lounge_probes=9,nonblocking_scenic_dock_probes=8,
        qualification='Exact runtime paving readback and native collision checks; frame review recorded separately.')
    previous_finish()
parent['finish']=finish
