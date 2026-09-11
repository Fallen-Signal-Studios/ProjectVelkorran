"""Read-only carrier clearance in fresh PIE or after the genuine Meeting receipt.

Call inspect(output_path=None, expected_phase='before_meeting'/'after_meeting').
No tick hook, input, authoring, movement, visibility, collision or campaign writes.
"""
import hashlib
import json
import math
import os
from pathlib import Path
import re
import traceback
import unreal

CHECKS = [('Selene carrier seam', (7000.,-4900.,100.), (7000.,-4450.,100.)),
          ('Selene connector length', (7000.,-8500.,100.), (7000.,-2600.,100.)),
          ('Selene east dogleg', (7000.,-6000.,100.), (5000.,0.,100.))]


def _valid(obj):
    return obj is not None and unreal.SystemLibrary.is_valid(obj)


def _path(obj):
    return obj.get_path_name() if _valid(obj) else None


def _xyz(value):
    return [float(value.x),float(value.y),float(value.z)]


def _hash(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _hit(value):
    if value is None:
        return {'blocking':False, 'native_return':'None (no blocking hit)'}
    candidates=value if isinstance(value,tuple) else (value,)
    hits=[v for v in candidates if isinstance(v,unreal.HitResult)]
    assert len(hits)==1, 'Unexpected native capsule output'
    parts=hits[0].to_tuple()
    return dict(blocking=bool(parts[0]), initial_overlap=bool(parts[1]),
                actor=_path(parts[9]), component=_path(parts[10]), raw=hits[0].export_text())


def inspect(output_path=None, expected_phase='before_meeting'):
    assert expected_phase in ('before_meeting','after_meeting')
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    assert _valid(world) and world.get_path_name().split('.')[0].replace('UEDPIE_0_','')=='/Game/Aurelion/Maps/L_Aurelion_M12'
    pc=unreal.GameplayStatics.get_player_controller(world,0)
    pawn=unreal.GameplayStatics.get_player_pawn(world,0)
    assert isinstance(pc,unreal.SovPlayerController) and _valid(pawn)
    assert isinstance(pawn,(unreal.SovTarrikCharacter,unreal.SovSeleneCharacter))
    assert pawn.is_character_ready() and pawn.is_alive() and pawn.get_health()>0
    assert pc.get_campaign_transition_state()==unreal.SovCampaignTransitionState.IDLE
    assert not unreal.GameplayStatics.is_game_paused(world)
    events=list(pc.get_campaign_state().get_journal())
    journal=[event.export_text() for event in events]
    meetings=[event for event in events if str(event.mission_id)=='M12_FireAndFrost' and str(event.beat_id)=='MeetingAndCarrierRescue']
    assert len(meetings)==(1 if expected_phase=='after_meeting' else 0), 'Genuine Meeting journal does not match requested observation phase'
    if meetings:
        guid=meetings[0].event_id.export_text()
        assert re.fullmatch('[0-9A-Fa-f]{32}',guid) and int(guid,16), 'Meeting receipt lacks a valid native event identity'
    root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
    protected=[root/'Content/Aurelion/Maps/L_Aurelion_Graybox_Velkorran.umap',root/'Content/Aurelion/Maps/L_Aurelion_M12.umap']
    before={str(p):_hash(p) for p in protected}
    destination=Path(output_path) if output_path else Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])/('CarrierClearance-'+expected_phase+'.json')
    assert not destination.exists(), 'Preserve previous clearance evidence'
    report=dict(status='running', phase=expected_phase, read_only=True, world=_path(world), pawn=_path(pawn),
        journal_before=journal, genuine_meeting_receipt=meetings[0].export_text() if meetings else None,
        position_before=_xyz(pawn.get_actor_location()), assets_before=before,
        parts=[], gates=[], paths=[], capsule_sweeps=[], errors=[], rendered_camera_framing_verified=False)
    try:
        if unreal.SovAurelionNavigationLibrary.is_navigation_being_built_or_locked(world):
            report['status']='pending_navigation_build'
        else:
            names={'Aurelion_Carrier_'+phase+suffix for phase in ('Suspended','Stable') for suffix in ('','_Engine-1','_Engine1','_Forward')}
            actors=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.StaticMeshActor)
            parts=[actor for actor in actors if actor.get_actor_label().startswith('Aurelion_Carrier_')]
            assert len(parts)==8 and {actor.get_actor_label() for actor in parts}==names, 'Carrier part identities are missing, duplicated or unexpected'
            after_meeting=bool(meetings)
            for actor in sorted(parts,key=lambda a:a.get_actor_label()):
                component=actor.static_mesh_component
                center,extent,_=unreal.SystemLibrary.get_component_bounds(component)
                stable=actor.get_actor_label().startswith('Aurelion_Carrier_Stable')
                should_show=stable==after_meeting
                row=dict(actor=_path(actor),label=actor.get_actor_label(),component=_path(component),center=_xyz(center),extent=_xyz(extent),
                    min_x=center.x-extent.x,actor_location=_xyz(actor.get_actor_location()),
                    profile=str(component.get_collision_profile_name()),collision=str(component.get_collision_enabled()),
                    nav_eligible=bool(component.get_editor_property('can_ever_affect_navigation')),
                    hidden=bool(actor.get_editor_property('hidden')),actor_collision=actor.get_actor_enable_collision(),expected_visible=should_show)
                report['parts'].append(row)
                assert row['min_x']>=7800.-.1, row['label']+': scenery footprint intrudes toward the corridor'
                assert component.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and row['profile']=='NoCollision'
                assert not row['nav_eligible'], row['label']+': scenery contributes to navigation'
                assert row['hidden']!=should_show and row['actor_collision']==should_show, row['label']+': journal visibility/actor flag ownership differs'
            for label,expected_blocking in (('Aurelion_CarrierApproachPresentation',not after_meeting),('Aurelion_CarrierRescuedPresentation',after_meeting)):
                gates=[g for g in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SovAurelionJournalGate) if g.get_actor_label()==label]
                assert len(gates)==1 and gates[0].is_blocking_route()==expected_blocking, 'Carrier gate differs from actual journal: '+label
                report['gates'].append(dict(actor=_path(gates[0]),label=label,blocking=gates[0].is_blocking_route()))
            capsule=pawn.get_editor_property('capsule_component')
            radius,half=float(capsule.get_scaled_capsule_radius()),float(capsule.get_scaled_capsule_half_height())
            assert math.isfinite(radius) and math.isfinite(half) and 0<radius<=half<300
            report['actual_player_capsule']=dict(component=_path(capsule),radius=radius,half_height=half)
            ignored=[]
            for character in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.NarrativeCharacter):
                ignored.append(character)
                ignored.extend(character.get_attached_actors())
                visual=character.get_character_visual()
                if _valid(visual) and visual not in ignored: ignored.append(visual)
            report['capsule_ignore_scope']='Mobile Narrative characters and their attachments; no carrier, wall, floor or other scenery is ignored'
            for name,start,end in CHECKS:
                path=unreal.SovAurelionNavigationLibrary.find_path_to_location_synchronously(world,unreal.Vector(*start),unreal.Vector(*end),pawn,None)
                row=dict(name=name,start=list(start),destination=list(end),valid=path.is_valid() if path else False,
                         partial=path.is_partial() if path else None,points=[_xyz(v) for v in path.path_points] if path else [])
                report['paths'].append(row)
                assert row['valid'] and not row['partial'], 'Carrier corridor still lacks a complete native path: '+name
                points=list(path.path_points)
                assert len(points)>=2
                for index,(a,b) in enumerate(zip(points,points[1:])):
                    first=unreal.Vector(a.x,a.y,a.z+half+2.)
                    last=unreal.Vector(b.x,b.y,b.z+half+2.)
                    result=_hit(unreal.SystemLibrary.capsule_trace_single_by_profile(world,first,last,radius,half,
                        unreal.Name('Pawn'),False,ignored,unreal.DrawDebugTrace.NONE,True))
                    report['capsule_sweeps'].append(dict(name=name,segment=index,start=_xyz(first),end=_xyz(last),result=result))
                    assert not result['blocking'], 'Actual player capsule is obstructed: '+name+' '+str(result)
            report['status']='passed_readonly_carrier_clearance'
    except Exception:
        report['status']='failed_inspection'
        report['errors'].append(traceback.format_exc())
    report['assets_after']={str(p):_hash(p) for p in protected}
    report['journal_after']=[entry.export_text() for entry in pc.get_campaign_state().get_journal()]
    report['position_after']=_xyz(pawn.get_actor_location())
    report['unchanged']=before==report['assets_after'] and journal==report['journal_after'] and report['position_before']==report['position_after']
    if not report['unchanged']:
        report['status']='failed_inspection'
        report['errors'].append('Actor position, journal or protected disk packages changed during the synchronous read-only probe')
    destination.write_text(json.dumps(report,indent=2),encoding='utf-8')
    unreal.log('AURELION_CARRIER_CLEARANCE '+report['status']+' '+str(destination))
    return report


if __name__=='__main__':
    inspect()
