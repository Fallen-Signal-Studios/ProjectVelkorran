"""Fresh architecture checks plus real pre-meeting carrier visibility/clearance.

Does not grant the Meeting receipt or move the player, actors or journal.
"""
from pathlib import Path
import hashlib,json,runpy,time,traceback,unreal
root=Path(unreal.Paths.project_dir()).resolve()
DEFER_CARRIER_REFINEMENT_AUTORUN=True
exec(compile((root/'Scripts/Editor/verify_carrier_refinement.py').read_text(),'verify_carrier_refinement','exec'),globals())
del DEFER_CARRIER_REFINEMENT_AUTORUN
saved=Path(unreal.Paths.project_saved_dir()).resolve()
assert saved.is_relative_to(root/'Saved/Validation/Aurelion') and not list((saved/'SaveGames').glob('*.sav'))
map_file=root/'Content/Aurelion/Maps/L_Aurelion_M12.umap'
map_hash=hashlib.sha256(map_file.read_bytes()).hexdigest()
vstate=dict(stage='editor',started=time.monotonic(),last=0)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
def finish(report):
    unreal.unregister_slate_post_tick_callback(vhandle)
    try:
        report['map_unchanged']=hashlib.sha256(map_file.read_bytes()).hexdigest()==map_hash
        assert report['map_unchanged']
        (out/'carrier-pie.json').write_text(json.dumps(report,indent=2))
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
    finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
def tick(delta):
    try:
        now=time.monotonic()
        assert now-vstate['started']<180,'Carrier PIE verification timed out'
        if vstate['stage']=='editor':
            if now-vstate['started']<15:return
            verify()
            assert unreal.GameUserSettings.get_game_user_settings().complete_accessibility_setup()
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            vstate.update(stage='pie',last=now)
            return
        if now-vstate['last']<10:return
        vstate['last']=now
        game=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if not game:return
        pawn=unreal.GameplayStatics.get_player_pawn(game,0)
        if not pawn or not pawn.is_character_ready():return
        pc=unreal.GameplayStatics.get_player_controller(game,0)
        if not isinstance(pc,unreal.SovPlayerController) or pc.get_campaign_transition_state()!=unreal.SovCampaignTransitionState.IDLE:return
        if unreal.SovAurelionNavigationLibrary.is_navigation_being_built_or_locked(game):return
        probe=runpy.run_path(str(root/'Scripts/Validation/Aurelion/probe_aurelion_carrier_clearance_readonly.py'),run_name='carrier_readonly_probe')
        result=probe['inspect'](str(out/'carrier-live-clearance.json'),expected_phase='before_meeting')
        assert result['status']=='passed_readonly_carrier_clearance',result
        live=list(unreal.GameplayStatics.get_all_actors_of_class(game,unreal.StaticMeshActor))
        by_label={a.get_actor_label():a for a in live}
        check=runpy.run_path(str(root/'Scripts/Editor/check_carrier_kit.py'))
        rows=[]
        for label,a in by_label.items():
            if not label.startswith('Aurelion_Carrier_'):continue
            c=a.get_component_by_class(unreal.InstancedStaticMeshComponent)
            assert c and c.get_instance_count()==1 and c.static_mesh.get_name()==check['mesh_name'](label)
            assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and not c.get_editor_property('can_ever_affect_navigation')
            t=c.get_instance_transform(0,world_space=True);expected=check['art_transform'](label,by_label)
            assert (t.translation-expected.translation).length()<.1 and t.rotation.angular_distance(expected.rotation)<.001
            assert a.get_editor_property('hidden')==('_Stable' in label)
            rows.append(dict(label=label,hidden=a.get_editor_property('hidden'),visual_transform=t.export_text()))
        assert len(rows)==8
        fill=json.loads((root/'Art/Source/Aurelion/CarrierKit/lighting-fit.json').read_text())
        runpy.run_path(str(root/'Scripts/Editor/fit_carrier_fill.py'))['check_carrier_fill'](live,fill)
        finish(dict(status='passed_before_meeting',parts=rows,clearance='carrier-live-clearance.json',
            qualification='Fresh native PIE boot and pre-meeting geometry/visibility/navigation only; rescued transition, full route and performance remain unqualified.'))
    except Exception:
        finish(dict(status='failed',error=traceback.format_exc()))
        raise
vhandle=unreal.register_slate_post_tick_callback(tick)
