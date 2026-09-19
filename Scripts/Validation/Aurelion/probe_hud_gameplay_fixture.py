"""Controlled M12 integration fixture: actual firearm ammo, hostile detection and Blackout.
Uses Narrative wield API and a spawned validation NPC, not physical input or mission progress.
Run in the isolated visible Aurelion editor runner. Writes no content assets.
"""
import unreal, os, json, time, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import validate_owned_weapon_hud_readonly as owned_hud
OUT=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
settings=unreal.GameUserSettings.get_game_user_settings()
original_settings=settings.get_settings_snapshot()
settings.complete_accessibility_setup()
report=dict(status='running',scope='Controlled enemy fixture and Narrative wield API; actual HUD/Blackout pipeline. No physical input or encounter completion coverage.',samples=[])
started=time.monotonic();phase='start';phase_at=started;target=None;item=None

def write():
    (OUT/'hud-gameplay-fixture.json').write_text(json.dumps(report,indent=2,default=str))
def stage(value):
    global phase,phase_at
    phase=value;phase_at=time.monotonic();report['phase']=value;write()
def cleanup():
    settings.apply_settings_snapshot(original_settings)
    if target and unreal.SystemLibrary.is_valid(target):target.destroy_actor()
    unreal.unregister_slate_post_tick_callback(handle)
def finish(error=None):
    report.update(status='failed' if error else 'passed',error=error)
    try:cleanup()
    finally:write()
def shot(world,name):
    unreal.SystemLibrary.execute_console_command(world,'Shot showui -nosuffix filename='+str(OUT/(name+'.png')))
def tick(delta):
    global target,item
    try:
        if time.monotonic()-started>180:raise AssertionError('Fixture timed out in '+phase)
        editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if phase=='start':
            editor.editor_request_begin_play();stage('ready');return
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if not world:return
        pawn=unreal.GameplayStatics.get_player_pawn(world,0)
        pc=unreal.GameplayStatics.get_player_controller(world,0)
        if not pawn or not isinstance(pc,unreal.SovPlayerController):return
        if phase=='ready':
            if not pawn.is_character_ready() or pawn.is_character_pending_load():return
            inventory=pawn.get_component_by_class(unreal.NarrativeInventoryComponent)
            item=inventory.find_item_of_class(unreal.load_class(None,'/Game/Items/Weapons/WI_Cinderline.WI_Cinderline_C'),False)
            assert item,'Cinderline missing from actual starting inventory'
            slot=unreal.GameplayTagLibrary.get_tag_name(item.get_editor_property('current_slot'))
            assert str(slot) not in ('','None'),'Cinderline is not equipped'
            state=unreal.WeaponWieldState()
            equip=unreal.GameplayTagContainer();assert equip.import_text('(GameplayTags=((TagName="%s")))'%slot)
            hands=unreal.GameplayTagContainer();assert hands.import_text('(GameplayTags=((TagName="Narrative.Equipment.WieldSlot.Mainhand")))')
            state.set_editor_property('equip_slots',equip);state.set_editor_property('equip_weapons',[item]);state.set_editor_property('wield_slots',hands)
            pawn.set_wield_state(state);stage('wield');return
        surfaces=[w for w in unreal.ObjectIterator(unreal.SovHolographicHUDSurface) if w.is_in_viewport() and w.get_world()==world]
        if not surfaces:return
        assert len(surfaces)==1
        surface=surfaces[0];view=surface.get_holographic_hud_view()
        elapsed=time.monotonic()-phase_at
        if phase=='wield':
            if item not in pawn.get_wielded_weapons() or elapsed<2:return
            assert view.has_ammo,'Wielded firearm did not expose ammo in HUD'
            clip=item.get_ammo_in_clip();reserve=item.get_spare_ammo()
            assert clip==item.get_clip_size() and clip+reserve==250,'Fresh Cinderline did not start loaded with conserved ammunition'
            assert view.ammo_in_clip==clip and view.ammo_reserve==reserve
            ammo_text=str(surface.get_editor_property('AmmoText').get_text())
            assert ammo_text==f'{clip} / {reserve}',ammo_text
            assert surface.get_editor_property('AmmoRegion').get_visibility()!=unreal.SlateVisibility.COLLAPSED
            readout = owned_hud.sample()
            assert readout['status'] == 'passed', str(readout)
            assert readout['legacy_readout_retired'], 'Legacy bottom-right ammo is still present'
            report['owned_hud_readback'] = readout
            report['samples'].append(dict(phase='ammo',weapon=item.get_class().get_path_name(),clip=clip,reserve=reserve,text=ammo_text,protagonist=str(unreal.GameplayTagLibrary.get_tag_name(view.protagonist))))
            # The real cinematic event restores direct canvas children. Exercise
            # both modes; authored retirement must survive the restore callback.
            hud=pc.get_narrative_gameplay_hud()
            for essential in (False, True):
                hud.set_hud_hidden(True,essential)
                hud.set_hud_hidden(False,essential)
            report['after_hide_show_events']=owned_hud.sample()
            # Those calls alone may not reproduce the full route's timing.
            # Explicitly exercise its observed visible-parent state as a separate
            # controlled presentation stress case, not a mission/handoff claim.
            children=[w for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.UserWidget,False)
                      if w.get_path_name()==readout['weapon_widget']]
            assert len(children)==1
            container=children[0].get_parent()
            assert container.get_name()=='VerticalBox_0'
            container.set_visibility(unreal.SlateVisibility.VISIBLE)
            report['restore_stress_method']='Explicit visible legacy parent after SetHUDHidden events; controlled UI state only'
            stage('hud_restored');return
        if phase=='hud_restored' and elapsed>2:
            readout=owned_hud.sample()
            report['after_cinematic_restore']=readout
            assert readout['legacy_readout_retired'], 'Visible legacy parent re-enabled weapon readout'
            assert readout['status']=='passed', str(readout)
            location=pawn.get_actor_location()+pawn.get_actor_forward_vector()*350
            target=unreal.SovMeleeValidationLibrary.spawn_validation_npc(world,unreal.load_asset('/Game/Aurelion/Enemies/NPC_AurelionEnforcer'),unreal.Transform(location,unreal.Rotator(),unreal.Vector(1,1,1)))
            assert target,'Validation Enforcer did not spawn'
            stage('detection');return
        detection=pawn.get_component_by_class(unreal.SovProximityDetectionComponent)
        if phase=='detection':
            controller=target.get_controller()
            if controller:
                controller.stop_movement()
                brain=controller.get_component_by_class(unreal.BrainComponent)
                if brain:brain.stop_logic('Isolated HUD detection fixture')
            if elapsed<4 or detection.get_live_sighting_count()<1:return
            assert len(view.contacts)>0,'Actual detection not presented'
            report['samples'].append(dict(phase='normal',detected=detection.get_contact_count(),live=detection.get_live_sighting_count(),presented=len(view.contacts)))
            shot(world,'hud-firearm-live-contact');stage('normal_capture');return
        if phase=='normal_capture' and elapsed>4:
            test=settings.get_settings_snapshot();test.set_editor_property('bModifierBlackout',True)
            settings.apply_settings_snapshot(test)
            assert settings.get_settings_snapshot().modifier_blackout
            stage('blackout');return
        if phase=='blackout' and elapsed>3:
            assert detection.get_contact_count()>0,'Fixture lost detection before Blackout comparison'
            assert len(view.contacts)==0,'Blackout left radar contacts visible'
            report['samples'].append(dict(phase='blackout',detected=detection.get_contact_count(),presented=0))
            shot(world,'hud-firearm-blackout');stage('blackout_capture');return
        if phase=='blackout_capture' and elapsed>4:
            settings.apply_settings_snapshot(original_settings);stage('restored');return
        if phase=='restored' and elapsed>3:
            if not original_settings.modifier_blackout:assert len(view.contacts)>0,'Radar did not restore after Blackout'
            report['samples'].append(dict(phase='restored',presented=len(view.contacts)))
            finish();return
    except Exception as exc:
        finish(str(exc));unreal.log_error('HUD_GAMEPLAY_FIXTURE '+str(exc))
write()
handle=unreal.register_slate_post_tick_callback(tick)
