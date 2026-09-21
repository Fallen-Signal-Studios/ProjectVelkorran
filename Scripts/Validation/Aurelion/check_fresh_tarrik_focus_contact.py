"""Earn E4A normally, retire player input, and observe Tarrik's public focus command.

No restored legacy kit, damage injection, teleport, grants or resource edits.
Commanded contact only; this is not passive AI or complete-campaign acceptance.
"""
from pathlib import Path
import hashlib,json,os,runpy,sys,time,traceback,unreal
here=Path(__file__).resolve().parent;sys.path.insert(0,str(here))
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
os.environ['SOV_AURELION_ROUTE_STOP_AFTER']='continue_aurelion_e4_entry_input'
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
maps=[root/'Content/Aurelion/Maps'/n for n in ('L_Aurelion_M12.umap','L_Aurelion_M13.umap')]
hashes={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in maps}
state=dict(phase='route',start=time.monotonic(),busy=False)
report=dict(status='running',scope=globals().get('CONTACT_SCOPE',__doc__))
with_player_hit=bool(globals().get('PLAYER_CONTRIBUTION_PROBE',False))
report['player_hit_probe']=with_player_hit
def write(): (out/'fresh-tarrik-focus.json').write_text(json.dumps(report,indent=2))
def finish(error=None):
    contact=sys.modules.get('observe_companion_after_player_shot')
    if contact and contact._RUN and contact._RUN.handle is not None:contact._RUN.stop('Wrapper finished')
    chain=sys.modules.get('continue_aurelion_route_input')
    if chain and chain._RUN and not chain._RUN.done:chain.stop()
    report.update(status='failed' if error else 'observed_requires_review',error=error,
        maps_unchanged=all(hashlib.sha256(p.read_bytes()).hexdigest()==hashes[str(p)] for p in maps))
    write();level.editor_request_end_play();state.update(phase='stopping',at=time.monotonic())
def tick(dt):
    if state['busy']:return
    state['busy']=True
    try:
        if state['phase']=='stopping':
            if not level.is_in_play_in_editor() and time.monotonic()-state['at']>3:
                unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
            return
        assert time.monotonic()-state['start']<1500,'Bounded route/contact deadline exceeded'
        chain=sys.modules.get('continue_aurelion_route_input')
        if state['phase']=='route':
            if not chain or not chain._RUN or not chain._RUN.done:return
            assert chain._RUN.report['status']=='passed',chain._RUN.report
            assert chain._RUN.handle is None and chain._RUN.child.handle is None
            import observe_companion_after_player_shot as contact
            contact.start(out,passive=not with_player_hit)
            probe=contact._RUN
            assert isinstance(probe.pawn,unreal.SovSeleneCharacter)
            assert 'Tarrik' in probe.companion.get_class().get_name()
            if with_player_hit:
                # Fund contribution on a durable enemy; leave the ordinary Linkbound
                # intact for Tarrik. Both targets are real, living encounter actors.
                elites=[a for a in unreal.GameplayStatics.get_all_actors_of_class(probe.world,unreal.NarrativeNPCCharacter)
                        if 'AurelionElite' in a.get_class().get_name() and a.is_alive()
                        and a.get_health()>150 and probe.pc.line_of_sight_to(a)
                        and unreal.ArsenalStatics.get_attitude(probe.pawn,a)==unreal.TeamAttitude.HOSTILE
                        and probe.pawn.get_distance_to(a)<5000]
                assert len(elites)==1,'Requires a living visible Elite for the single ordinary player shot'
                probe.player_target=elites[0]
                report['player_target']=probe.player_target.get_path_name()
            tags=unreal.GameplayTagLibrary.get_owned_gameplay_tags(probe.target).export_text()
            assert 'Narrative.State.Invulnerable' not in tags and probe.target.is_alive(),tags
            report.update(target=probe.target.get_path_name(),initial_target_tags=tags,
                initial_target_health=probe.target.get_health(),companion=probe.companion.get_path_name(),
                command_result=str(probe.companion.get_companion_component().request_command(
                    probe.pawn,unreal.SovCompanionCommand.FOCUS_TARGET,probe.target)))
            state['phase']='contact';write();return
        import observe_companion_after_player_shot as contact
        if contact._RUN.handle is not None:return
        report['damage_receipts']=contact._RUN.report['companion_damage']
        report['player_damage_details']=contact._RUN.report.get('player_damage_details',[])
        report['companion_damage_details']=contact._RUN.report.get('companion_damage_details',[])
        report['observer_errors']=contact._RUN.report['errors']
        assert not report['observer_errors'],report['observer_errors']
        if with_player_hit:
            player_hits=[r for r in report['player_damage_details'] if r['target']==report['player_target']
                         and r['health']+r['shield']>0 and r['target_alive'] and not r['fatal']]
            assert player_hits,'No verified nonlethal ordinary player hit; companion response is unqualified'
            report['companion_health_damage_after_player_hit']=sum(r['health'] for r in report['companion_damage_details']
                if r['target']==report['target'] and r['elapsed']>player_hits[0]['elapsed'])
        finish()
    except Exception:finish(traceback.format_exc())
    finally:state['busy']=False
handle=unreal.register_slate_post_tick_callback(tick);write()
runpy.run_path(str(here/'start_companion_mesh_route.py'))
