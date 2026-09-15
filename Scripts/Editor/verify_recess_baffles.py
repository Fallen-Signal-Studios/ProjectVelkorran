"""Stopped-editor admission for refuge geometry; never grants runtime acceptance."""
import json
import math
import runpy
import sys
from pathlib import Path
import unreal

root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / 'Scripts/Editor'))
from story_content import SCENES
from validate_aurelion_handoff_destinations import validate_handoff_destinations
from validate_aurelion_scene_requests import validate_scene_request_surfaces

unreal._recess_probe_stage = 'verified-geometry'
probe = runpy.run_path(str(root / 'Scripts/Editor/probe_recess_baffles.py'))
report = probe['report']
world = probe['world']
out = root / 'Saved/Validation/Aurelion/RecessBaffles-20260913'
report.update(status='checking', runtime_survivor_acceptance=False)
try:
    by_label={a.get_actor_label():a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()}
    expected=[('West'+str(i),x,1.) for i,x in enumerate((-2900,-2700,-2500,-2300))]
    expected += [('East0',2950,.5),('East1',3050,.5),('East2',3150,.5)]
    report['baffles']=[]
    for suffix,x,scale in expected:
        actor=by_label['ART_RefugeBaffle_'+suffix]; c=actor.static_mesh_component
        assert (actor.get_actor_location()-unreal.Vector(x,22060,-1200)).length()<.1
        mesh_name=c.static_mesh.get_name()
        if mesh_name=='SM_Aurelion_RecessPanel_2m':
            assert c.static_mesh.get_path_name()=='/Game/Aurelion/Environment/Blender/SM_Aurelion_RecessPanel_2m.SM_Aurelion_RecessPanel_2m'
            assert (actor.get_actor_scale3d()-unreal.Vector(scale,1,1)).length()<.001
        else:
            width=1 if scale==.5 else 2
            assert mesh_name=='SM_Aurelion_KIT_RefugePanel_'+str(width)+'m'
            assert (actor.get_actor_scale3d()-unreal.Vector(1,1,1)).length()<.001
        assert c.get_collision_enabled()==unreal.CollisionEnabled.QUERY_AND_PHYSICS
        report['baffles'].append(dict(label=actor.get_actor_label(),transform=actor.get_actor_transform().export_text()))
    assert not report['navigation_pending']
    assert all(p['complete'] for p in report['paths'])
    assert all(not p['blocked'] for p in report['cast_capsules'])
    assert all(p['blocked'] for p in report['sightlines'])
    heroes = {h: {'pawn': unreal.load_asset('/Game/PlayerCharacters/BP_Sov'+h).generated_class()}
              for h in ('Tarrik', 'Selene')}
    context = dict(chapter=12, world=world, heroes=heroes)
    report['handoffs'] = {}
    validate_handoff_destinations(context, report['handoffs'])
    requests = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovAurelionRequestActor)
                if a.operation == unreal.SovAurelionRequest.PLAY_SCENE]
    manifest = {row[0]: row[2] for row in SCENES[:8]}
    scene_report = {'scenes': {str(a.beat_id): dict(actor=a.story.get_path_name(), hero=manifest[str(a.beat_id)]) for a in requests}}
    report['scenes'] = scene_report
    validate_scene_request_surfaces(context, scene_report)
    caches = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovAurelionMedicalCache)
    assert len(caches) == 1
    cache = caches[0]
    supports = unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SovAurelionPrioritySupport)
    assert len(supports)==1 and cache.support==supports[0]
    assert (cache.get_actor_location()-unreal.Vector(-3050,22600,-1140)).length()<.1
    assert (supports[0].west_cache_barrier.get_world_location()-unreal.Vector(-3050,22475,-1060)).length()<.1
    report['medical_scope'] = 'Reachable outside closed cabinet; visibility conditional on native West priority opening its gate. No gate or campaign state changed.'
    report['medical_approaches'] = []
    for name, hero in heroes.items():
        cdo = unreal.get_default_object(hero['pawn'])
        capsule = cdo.get_editor_property('capsule_component')
        radius = capsule.get_scaled_capsule_radius()
        half = capsule.get_scaled_capsule_half_height()
        target = cache.get_actor_location()
        samples = []
        for angle in range(0,360,15):
            candidate = unreal.Vector(target.x+220*math.cos(math.radians(angle)),
                target.y+220*math.sin(math.radians(angle)), -1110)
            path = unreal.SovAurelionNavigationLibrary.find_path_to_location_synchronously(
                world, unreal.Vector(-2600,21750,-1110), candidate, None, None)
            if not path or not path.is_valid() or path.is_partial():
                samples.append(dict(angle=angle, reachable=False)); continue
            endpoint = path.path_points[-1]
            body = unreal.Vector(endpoint.x, endpoint.y, -1200+half+2)
            eye = unreal.Vector(body.x, body.y, body.z+cdo.get_editor_property('base_eye_height'))
            distance = (body-target).length()
            los = probe['hit'](unreal.SystemLibrary.line_trace_single(world,eye,target,
                unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[cache,supports[0]],unreal.DrawDebugTrace.NONE,True))
            closed_los = probe['hit'](unreal.SystemLibrary.line_trace_single(world,eye,target,
                unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[cache],unreal.DrawDebugTrace.NONE,True))
            clearance = probe['hit'](unreal.SystemLibrary.capsule_trace_single_by_profile(world,body,
                unreal.Vector(body.x,body.y,body.z+.01),radius,half,'Pawn',False,[],unreal.DrawDebugTrace.NONE,True))
            samples.append(dict(angle=angle,reachable=True,body=body.export_text(),eye=eye.export_text(),
                distance_cm=distance,line_of_sight=los,closed_gate_line_of_sight=closed_los,capsule=clearance,
                accepted=distance<=250 and not los['blocked'] and not clearance['blocked']))
        row = dict(hero=name,radius=radius,half_height=half,samples=samples,
                   accepted=any(s.get('accepted',False) for s in samples))
        report['medical_approaches'].append(row)
        assert row['accepted'], 'No reachable visible cache approach for '+name
        assert all(s['closed_gate_line_of_sight'].get('actor')==supports[0].get_actor_label()
                   for s in samples if s.get('accepted')), 'Expected native closed priority gate'
    report['status'] = 'PASS_STATIC_GEOMETRY'
except Exception as error:
    report.update(status='FAILED', error=str(error))
    raise
finally:
    (out/'admission.json').write_text(json.dumps(report,indent=2),encoding='utf8')
unreal.log('RECESS_BAFFLES_STATIC_ADMISSION_PASS; runtime remains unqualified')
