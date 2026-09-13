"""Real PIE entry check. Only normal Enhanced Input, unchanged standard profile setup,
and editor begin/end-play requests are issued. No combat/campaign/position writes."""
import hashlib
import json
import math
import os
import sys
from pathlib import Path
import time
import traceback
import unreal
sys.path.insert(0,str(Path(__file__).resolve().parent))
from inspect_aurelion_enemies_pie import inspect_enemies
from aurelion_wheel_input import Selector
import validate_owned_weapon_hud_readonly as hud_readonly
from inspect_aurelion_components_readonly import run as inspect_components
from probe_aurelion_carrier_clearance_readonly import inspect as inspect_carrier_clearance
from probe_aurelion_navigation_readonly import inspect as inspect_navigation

OUT = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
SAVED = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())).resolve()
assert SAVED == (OUT/'UserData/Saved').resolve(), 'Requires isolated fresh validation profile'
assert not list((SAVED/'SaveGames').glob('*.sav')), 'Profile is not fresh'
ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
MAP = '/Game/Aurelion/Maps/L_Aurelion_M12'
START = time.monotonic()
report = dict(status='running', scope='M12 fresh entry, genuine arrival hold, E1 physical entry',
              method='Normal per-frame Enhanced Input in real rendered PIE',
              physical_keyboard_validation=False, rendered_image_review=False,
              direct_journal_or_damage_or_position_writes=False, samples=[], stages=[], holds=[])
ctl = dict(phase='initializing', phase_at=START, handle=None, done=False, stopping=False,
           sampled=0., written=0., hold_at=None, saw_countdown=False)
protected = list((ROOT/'Content/Aurelion').rglob('*.umap')) + list((ROOT/'Content/Aurelion').rglob('*.uasset'))
before = {str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in protected}
report['hud_validator'] = dict(path=str(Path(hud_readonly.__file__).resolve()), sha256=hashlib.sha256(Path(hud_readonly.__file__).read_bytes()).hexdigest())
actions = {name:unreal.load_asset('/NarrativePro/Pro/Core/Data/Input/'+name)
           for name in ('IA_Move','IA_Look','IA_Interact','IA_WeaponWheel')}
assert all(actions.values()), 'Existing Narrative input actions unavailable'

def write():
    report['elapsed_seconds']=round(time.monotonic()-START,3)
    tmp=OUT/'entry-result.tmp'
    tmp.write_text(json.dumps(report,indent=2,default=str),encoding='utf8')
    tmp.replace(OUT/'entry-result.json')

def stage(name, detail=None):
    ctl['phase']=name; ctl['phase_at']=time.monotonic()
    report['stages'].append(dict(phase=name,elapsed=ctl['phase_at']-START,detail=detail)); write()

def owned_hud_gate(key):
    gate = report.setdefault('owned_hud_gates', {}).setdefault(key, {'started': time.monotonic(), 'samples': 0, 'passed': False})
    if gate['passed']: return True
    observed = hud_readonly.sample()
    gate['samples'] += 1; gate.setdefault('first_sample', observed); gate['last_sample'] = observed
    gate['elapsed'] = time.monotonic() - gate['started']
    assert observed['status'] != 'failed', 'Owned HUD structural failure: '+str(observed)
    assert gate['elapsed'] <= 1., 'Owned HUD did not settle within one second: '+str(observed)
    gate['passed'] = observed['status'] == 'passed' and observed.get('shield', {}).get('qualification') == 'rounded_value_matches'
    return gate['passed']

def finish(passed, reason):
    if ctl['done']: return
    ctl['done']=True; report['status']='passed' if passed else 'failed'; report['reason']=reason
    after={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in protected}
    report['assets_unchanged']=after==before
    report['asset_hashes_before']=before; report['asset_hashes_after']=after
    if after!=before: report['status']='failed'
    report['hud_validator_unchanged'] = hashlib.sha256(Path(hud_readonly.__file__).read_bytes()).hexdigest() == report['hud_validator']['sha256']
    if not report['hud_validator_unchanged']: report['status']='failed'
    report['save_files']=[dict(name=p.name,bytes=p.stat().st_size) for p in (SAVED/'SaveGames').glob('*.sav')]
    stage('shutdown',reason)

def input_owner(world):
    engine=unreal.GameplayStatics.get_game_instance(world).get_outer()
    matches=[s for s in unreal.ObjectIterator(unreal.EnhancedInputLocalPlayerSubsystem)
             if isinstance(s.get_outer(),unreal.LocalPlayer) and s.get_outer().get_outer()==engine]
    assert len(matches)==1, 'Requires unique local-player input subsystem in this dedicated editor'
    return matches[0]

def inject(owner,move=(0.,0.),look=(0.,0.),interact=0.,wheel=0.):
    for name,v in (('IA_Move',(*move,0.)),('IA_Look',(*look,0.)),('IA_Interact',(interact,0.,0.)),('IA_WeaponWheel',(wheel,0.,0.))):
        owner.inject_input_vector_for_action(actions[name],unreal.Vector(*v),[],[])

def move(pc,pawn,owner,xy):
    p=pawn.get_actor_location(); dx,dy=xy[0]-p.x,xy[1]-p.y; distance=math.hypot(dx,dy)
    if distance<20.: inject(owner); return True
    yaw=math.radians(pc.get_control_rotation().yaw)
    forward=dx*math.cos(yaw)+dy*math.sin(yaw); right=-dx*math.sin(yaw)+dy*math.cos(yaw)
    strength=min(.8,max(.12,distance/160.))
    inject(owner,move=(right/distance*strength,forward/distance*strength)); return False

def aim(world,pc,owner,actor):
    camera=unreal.GameplayStatics.get_player_camera_manager(world,0)
    eye=camera.get_camera_location(); rotation=camera.get_camera_rotation(); target=actor.get_actor_location()
    dx,dy,dz=target.x-eye.x,target.y-eye.y,target.z-eye.z
    yaw=(math.degrees(math.atan2(dy,dx))-rotation.yaw+180.)%360.-180.
    pitch=(math.degrees(math.atan2(dz,math.hypot(dx,dy)))-rotation.pitch+180.)%360.-180.
    legacy=unreal.InputSettings.get_input_settings().get_editor_property('enable_legacy_input_scales')
    ys=pc.get_deprecated_input_yaw_scale() if legacy else 1.
    ps=pc.get_deprecated_input_pitch_scale() if legacy else 1.
    assert abs(ys)>.001 and abs(ps)>.001
    clamp=lambda v:max(-.7,min(.7,v))
    inject(owner,look=(clamp(yaw*.12/ys),clamp(pitch*.12/ps)))
    return abs(yaw)<3. and abs(pitch)<3.

def journal(state):
    return [dict(mission=str(e.mission_id),beat=str(e.beat_id),id=e.event_id.export_text(),sequence=e.sequence)
            for e in state.get_journal()]

def tick(dt):
    try:
        editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if ctl['done']:
            if report['status']=='passed' and os.environ.get('SOV_AURELION_ENTRY_KEEP_OPEN')=='1':
                report['continues_live_session']=True
                report['input_callback_retired']=True
                write()
                unreal.unregister_slate_post_tick_callback(ctl['handle'])
                ctl['handle']=None
                if os.environ.get('SOV_AURELION_ENTRY_CONTINUE_E1')=='1':
                    import continue_aurelion_e1_input
                    continue_aurelion_e1_input.start(OUT/'E1Continuation')
                    report['e1_continuation_started']=True
                    write()
                return
            if not ctl['stopping']:
                ctl['stopping']=True
                if editor.is_in_play_in_editor(): editor.editor_request_end_play(); return
            if not editor.is_in_play_in_editor() or time.monotonic()-ctl['phase_at']>15.:
                unreal.unregister_slate_post_tick_callback(ctl['handle'])
                unreal.EditorPythonScripting.set_keep_python_script_alive(False)
            return
        entry_limit=360. if os.environ.get('SOV_AURELION_ENTRY_CONTINUE_E1')=='1' else 240.
        assert time.monotonic()-START<entry_limit, 'Entry validation deadline exceeded'
        phase_limit=180. if ctl['phase']=='await_cinderline_selection' else 80.
        assert time.monotonic()-ctl['phase_at']<phase_limit, 'Entry stage deadline exceeded: '+ctl['phase']
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if world is None: return
        assert '/Aurelion/Maps/UEDPIE_' in world.get_path_name(), 'Escaped Aurelion PIE map'
        pc=unreal.GameplayStatics.get_player_controller(world,0); pawn=unreal.GameplayStatics.get_player_pawn(world,0)
        report['startup']=dict(world=world.get_path_name(),controller=pc.get_path_name() if pc else None,
            pawn=pawn.get_path_name() if pawn else None,pawn_class=pawn.get_class().get_path_name() if pawn else None)
        if time.monotonic()-ctl['written']>1.: ctl['written']=time.monotonic(); write()
        if not isinstance(pc,unreal.SovPlayerController) or not isinstance(pawn,unreal.SovPlayerCharacterBase): return
        state=pc.get_campaign_state(); mission=state.get_active_mission() if state else None
        if not mission: return
        assert str(mission.mission_id)=='M12_FireAndFrost', 'Incorrect initial mission'
        events=journal(state)
        directors=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SovEncounterDirector)
        objectives=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SovCampaignEncounterObjective)
        e1=[d for d in directors if str(d.encounter_id)=='M12_E1_PressureHall']
        assert len(e1)==1, 'Missing or duplicate E1 director'
        e1=e1[0]
        now=time.monotonic()
        if now-ctl['sampled']>.25:
            ctl['sampled']=now
            report['samples'].append(dict(elapsed=now-START,phase=ctl['phase'],pawn=pawn.get_path_name(),
                position=pawn.get_actor_location().export_text(),health=pawn.get_health(),ready=pawn.is_character_ready(),
                transition=str(pc.get_campaign_transition_state()),journal=events,
                encounters={str(d.encounter_id):str(d.get_encounter_state()) for d in directors},
                entry_holds={str(d.encounter_id):d.get_editor_property('pre_entry_hold_error') for d in directors},
                entry_objectives=[dict(beat=str(o.completion_beat),error=o.last_error,
                    overlapping=o.start_volume.is_overlapping_actor(pawn),position=o.get_actor_location().export_text(),
                    extent=o.start_volume.get_scaled_box_extent().export_text()) for o in objectives],
                hold_remaining=pc.get_interaction_component().get_editor_property('remaining_interact_time')))
        if ctl['phase']=='walk_to_pressure_hall' and 'component_inspection' not in report:
            census=inspect_components(OUT/'component-startup-readonly')
            assert census['actors']==24 and not census['missing_roles'], 'Incomplete actual enemy component census'
            for rows in census['roles'].values():
                for row in rows:
                    for family in ('NarrativeAbilitySystemComponent','SovStatusComponent'):
                        assert len(row['families'][family])==1, 'Duplicate or absent required component: '+row['actor']+' '+family
            elites=census['roles']['Elite']
            assert len(elites)==1 and len(elites[0]['families']['SovWeakPointComponent'])==1 and elites[0]['native_core_valid'], 'Actual Elite must have exactly its valid native Core'
            report['component_inspection']=dict(status=census['status'],actors=census['actors'],missing_roles=census['missing_roles'],elite=elites[0])
        if ctl['phase']=='walk_to_pressure_hall' and 'enemy_inspection' not in report:
            inspected=inspect_enemies(world,OUT/'enemy-startup-readonly.json',require_initialized=True)
            report['enemy_inspection']={k:inspected.get(k) for k in ('status','contract_failures','startup_pending','inspection_errors')}
        if now-ctl['written']>1.: ctl['written']=now; write()
        assert pawn.is_alive(), 'Player died during entry validation'
        if not pawn.is_character_ready() or not state.is_state_valid() or pc.get_campaign_transition_state()!=unreal.SovCampaignTransitionState.IDLE: return
        owner=input_owner(world)
        if ctl['phase']=='await_cinderline_selection':
            # CharacterReady can precede completion of Narrative's appearance/weapon
            # streaming. Keep the existing bounded entry-stage deadline, but do not
            # spend the wheel's input deadline before the native load gate admits it.
            if pawn.is_character_pending_load():
                report['selection_waiting_for_native_load'] = True
                return
            report['selection_waiting_for_native_load'] = False
            if 'enemy_before_selection' not in report:
                inspected=inspect_enemies(world,OUT/'enemy-before-selection-readonly.json',require_initialized=True)
                report['enemy_before_selection']={k:inspected.get(k) for k in ('status','contract_failures','startup_pending','inspection_errors')}
            selected=[w for w in pawn.get_wielded_weapons() if w and w.get_class().get_path_name()=='/Game/Items/Weapons/WI_Cinderline.WI_Cinderline_C']
            if 'weapon_selector' not in ctl:
                ctl['weapon_selector']=Selector('/Game/Items/Weapons/WI_Cinderline.WI_Cinderline_C')
            held,wheel_report=ctl['weapon_selector'].step(world)
            inject(owner,wheel=1. if held else 0.)
            report['weapon_wheel']=wheel_report
            assert not ctl['weapon_selector'].done or wheel_report['status']=='passed', wheel_report.get('reason')
            if len(selected)==1 and ctl['weapon_selector'].done and wheel_report['status']=='passed':
                report['weapon_selection']=dict(item=selected[0].get_path_name(),method='Observed current owned Cinderline wield state; no inventory/equip writes')
                # Finish streaming the complete placed roster before walking into
                # the active encounter. Keep pending samples and the existing
                # 180-second stage bound; structural failures still fail at once.
                if now-ctl.get('roster_ready_sample_at',0.)<1.: return
                ctl['roster_ready_sample_at']=now
                inspected=inspect_enemies(world,OUT/'enemy-pre-entry-readiness.json',require_initialized=True)
                report.setdefault('pre_entry_roster_readiness',[]).append(dict(elapsed=now-START,
                    status=inspected['status'],pending=inspected.get('startup_pending',[])))
                assert not inspected.get('contract_failures') and not inspected.get('inspection_errors'), 'Placed roster structural failure'
                if inspected.get('startup_pending'): return
                assert inspected['status']=='passed_observable_startup_contract'
                stage('walk_to_pressure_hall')
            return
        if unreal.GameplayStatics.is_game_paused(world): return
        if ctl['phase']=='wait_ready':
            instance=unreal.GameplayStatics.get_game_instance(world)
            saves=[s for s in unreal.ObjectIterator(unreal.SovSaveSubsystem) if s.get_outer()==instance]
            assert len(saves)==1
            slots=saves[0].list_slots()
            found=[s for s in slots if str(s.boundary_id)=='Aurelion.CP0']
            if not found: return
            if unreal.SovAurelionNavigationLibrary.is_navigation_being_built_or_locked(world): return
            report['pie_navigation']=inspect_navigation(world)
            assert report['pie_navigation']['passed'], 'Durable navigation profile or bounded entry path failed in actual PIE'
            assert not events, 'Campaign advanced before arrival input'
            assert all(d.get_encounter_state()==unreal.SovEncounterState.INACTIVE for d in directors), 'Encounter started before entry'
            report['cp0']=found[0].export_text()
            report['interact_keys']=[k.export_text() for k in owner.query_keys_mapped_to_action(actions['IA_Interact'])]
            inject(owner)
            if time.monotonic()-ctl.get('carrier_probe_at',0.)<1.: return
            ctl['carrier_probe_at']=time.monotonic()
            attempts=report.setdefault('carrier_clearance_attempts',[])
            evidence=OUT/('carrier-before-meeting-readonly-%03d.json'%(len(attempts)+1))
            clearance=inspect_carrier_clearance(evidence,'before_meeting')
            row=dict(status=clearance['status'],unchanged=clearance['unchanged'],path=str(evidence),
                     sha256=hashlib.sha256(evidence.read_bytes()).hexdigest(),
                     elapsed=time.monotonic()-START)
            attempts.append(row)
            report['carrier_clearance_before_meeting']=row
            write()
            assert clearance['unchanged'], 'Carrier inspection changed the journal, pawn position or protected packages'
            if clearance['status']=='pending_navigation_build': return
            assert clearance['status']=='passed_readonly_carrier_clearance', 'Pre-Meeting carrier clearance failed: '+str(clearance['errors'])
            stage('walk_to_arrival')
        elif ctl['phase']=='walk_to_arrival':
            if move(pc,pawn,owner,(-6680.,-20200.)): stage('aim_arrival')
        elif ctl['phase']=='aim_arrival':
            terminals=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SovCampaignInteractionTerminal)
                       if str(a.terminal_id)=='Aurelion_TarrikArrival']
            assert len(terminals)==1
            terminal=terminals[0]; interaction=pc.get_interaction_component(); component=terminal.interactable
            aligned=aim(world,pc,owner,terminal); admission=component.can_interact(pawn,interaction)
            focus=interaction.get_editor_property('viewed_interactable')
            report['last_admission']=dict(aligned=aligned,reason=str(admission),native_action_text=str(component.get_interactable_action_text(pawn,interaction)),focus=focus.get_path_name() if focus else None)
            if aligned and focus==component and admission is not None:
                ctl['hold_at']=unreal.GameplayStatics.get_time_seconds(world)
                ctl['saw_countdown']=False; stage('hold_arrival')
        elif ctl['phase']=='hold_arrival':
            remaining=pc.get_interaction_component().get_editor_property('remaining_interact_time')
            if 0.<remaining<=.35: ctl['saw_countdown']=True
            if events:
                assert len(events)==1 and events[0]['beat']=='TarrikArrival', 'Unexpected arrival journal'
                assert ctl['saw_countdown'], 'No actual native hold countdown observed'
                report['holds'].append(dict(beat='TarrikArrival',countdown=True,journal=events))
                inject(owner); stage('step_clear_of_arrival')
            else: inject(owner,interact=1.)
        elif ctl['phase']=='step_clear_of_arrival':
            if move(pc,pawn,owner,(-7000.,-20200.)): stage('wait_open_gate_navigation')
        elif ctl['phase']=='wait_open_gate_navigation':
            inject(owner)
            gates=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SovAurelionJournalGate)
                   if str(a.beat_id)=='TarrikArrival' and a.use_gate_body]
            assert len(gates)==1, 'Arrival gate identity must be unique'
            path=unreal.SovAurelionNavigationLibrary.find_path_to_location_synchronously(world,
                unreal.Vector(-7000.,-19000.,0.),unreal.Vector(-7000.,-18600.,0.),pawn,None)
            complete=path is not None and path.is_valid() and not path.is_partial()
            report['arrival_gate_navigation']=dict(blocking=gates[0].is_blocking_route(),
                complete=complete,building=unreal.SovAurelionNavigationLibrary.is_navigation_being_built_or_locked(world),
                points=[p.export_text() for p in path.path_points] if path else [])
            if complete and not gates[0].is_blocking_route():
                stage('await_cinderline_selection' if os.environ.get('SOV_AURELION_ENTRY_CONTINUE_E1')=='1' else 'walk_to_pressure_hall')
        elif ctl['phase']=='walk_to_pressure_hall':
            rejected=[o.last_error for o in objectives if str(o.completion_beat)=='PressureHall'
                      and o.start_volume.is_overlapping_actor(pawn) and ('failed canonical combat-resource capture' in o.last_error
                      or 'failed Narrative actor/component record capture' in o.last_error
                      or 'could not serialize its Narrative spawn metadata' in o.last_error)]
            if rejected:
                first=ctl.setdefault('record_rejection_at',time.monotonic())
                report['encounter_record_rejection']=rejected[0]
                if time.monotonic()-first>2.: raise RuntimeError(rejected[0])
            else: ctl.pop('record_rejection_at',None)
            if e1.get_encounter_state()==unreal.SovEncounterState.ACTIVE:
                inject(owner)
                if not owned_hud_gate('tarrik_entry'): return
                participants=list(e1.participants)
                required=[p.character for p in participants if p.required_for_victory]
                assert len(required)==6, 'E1 roster differs from the six authored drones'
                assert sum(not n.get_editor_property('hidden') for n in required)==4, 'Initial wave must release four actual drones'
                assert all(n.is_alive() for n in required), 'Entry unexpectedly created a death'
                inspected=inspect_enemies(world,OUT/'enemy-entry-readonly.json',require_initialized=True)
                report['enemy_entry_inspection']={k:inspected.get(k) for k in ('status','contract_failures','startup_pending','inspection_errors')}
                assert inspected['status']=='passed_observable_startup_contract', 'Actual placed-enemy startup inspection did not pass: '+str(report['enemy_entry_inspection'])
                report['e1_entry']=dict(attempt=e1.get_attempt_id().export_text(),roster=[n.get_path_name() for n in required])
                from inspect_reserved_visuals_readonly import inspect as inspect_reserved_visuals
                visibility=inspect_reserved_visuals(world,OUT/'reserved-visuals-entry-readonly.json')
                assert visibility['status']=='completed_readonly', 'Reserved visual inspection failed'
                exposed=[r['participant_id'] for r in visibility['participants']
                         if r['director']==str(e1.encounter_id) and r.get('alive')
                         and r.get('hidden_character_has_exposed_visual')]
                report['e1_entry']['reserved_visuals_exposed']=exposed
                assert not exposed, 'Reserved wave has visible separate Narrative bodies: '+str(exposed)
                finish(True,'Fresh CP0, ordinary movement and arrival hold, then real E1 entry with four active and two reserved drones verified; combat and the remaining route are not qualified.')
            else: move(pc,pawn,owner,(-7000.,-17300.))
    except Exception:
        report['error']=traceback.format_exc(); finish(False,report['error'])

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
try:
    settings=unreal.GameUserSettings.get_game_user_settings()
    assert isinstance(settings,unreal.SovGameUserSettings)
    snapshot=settings.get_settings_snapshot()
    assert not snapshot.tap_interactions and abs(snapshot.interaction_hold_scale-1.)<.001
    assert settings.complete_accessibility_setup(), 'Cannot accept unchanged standard settings in isolated profile'
    report['settings']=snapshot.export_text()
    editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    assert not editor.is_in_play_in_editor()
    assert editor.load_level(MAP)
    assert editor.get_viewport_config_keys(), 'No real PIE viewport'
    stage('wait_ready'); ctl['handle']=unreal.register_slate_post_tick_callback(tick)
    editor.editor_request_begin_play()
except Exception:
    report['error']=traceback.format_exc(); finish(False,report['error'])
    if ctl['handle'] is None: unreal.EditorPythonScripting.set_keep_python_script_alive(False)
