"""Actual attack/reload ability input and visible split-ammo readback in isolated PIE.

Does not mutate ammo or save assets. This bypasses physical device routing.
"""
from pathlib import Path
import json,os,sys,time,unreal
sys.path.insert(0,str(Path(__file__).resolve().parent))
import probe_hud_gameplay_fixture as fixture
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
report=dict(status='running',samples=[],qualification='Cinderline native ability input and visible text; no physical input or Selene handoff coverage.')
phase='fixture';at=time.monotonic();busy=False;ended=None
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
def tag(name):
    t=unreal.GameplayTag();assert t.import_text('(TagName="'+name+'")');return t
def stage(name):
    global phase,at
    phase=name;at=time.monotonic()
def tick(dt):
    global busy,ended
    if busy:return
    busy=True
    try:
        if phase=='fixture':
            if fixture.report['status']=='running':return
            assert fixture.report['status']=='passed',fixture.report
            stage('start')
        if phase=='end':
            if ended is None:
                (out/'hud-fire-reload.json').write_text(json.dumps(report,indent=2))
                unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play();ended=time.monotonic()
            elif time.monotonic()-ended>2:
                unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False)
            return
        assert time.monotonic()-at<20,'Timed out in '+phase
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        pawn=unreal.GameplayStatics.get_player_pawn(world,0);asc=pawn.get_narrative_ability_system_component();item=fixture.item
        surface=next(w for w in unreal.ObjectIterator(unreal.SovHolographicHUDSurface) if w.is_in_viewport() and w.get_world()==world)
        def sample(label):
            clip=item.get_ammo_in_clip();reserve=item.get_spare_ammo();view=surface.get_holographic_hud_view()
            visible_clip=str(surface.get_editor_property('AmmoClip').get_text());visible_reserve=str(surface.get_editor_property('AmmoReserve').get_text())
            row=dict(phase=label,clip=clip,reserve=reserve,visible_clip=visible_clip,visible_reserve=visible_reserve)
            report['samples'].append(row)
            assert view.has_ammo and view.ammo_in_clip==clip and view.ammo_reserve==reserve,row
            assert visible_clip==str(clip) and visible_reserve==str(reserve),row
            return clip,reserve
        if phase=='start':
            report['initial']=sample('loaded')
            asc.ability_input_tag_pressed(tag('Narrative.Input.Attack'));stage('firing')
        elif phase=='firing' and time.monotonic()-at>1:
            asc.ability_input_tag_released(tag('Narrative.Input.Attack'));stage('fired_settle')
        elif phase=='fired_settle' and time.monotonic()-at>1:
            clip,reserve=sample('after_fire');initial=report['initial']
            assert 0<clip<initial[0] and reserve==initial[1],'Attack did not consume clip ammunition'
            report['spent']=initial[0]-clip
            unreal.SystemLibrary.execute_console_command(world,'Shot showui -nosuffix filename='+str(out/'hud-after-fire.png'))
            asc.ability_input_tag_pressed(tag('Narrative.Input.Reload'));asc.ability_input_tag_released(tag('Narrative.Input.Reload'));stage('reloading')
        elif phase=='reloading' and time.monotonic()-at>5:
            clip,reserve=sample('after_reload');assert clip==item.get_clip_size() and reserve==report['initial'][1]-report['spent']
            unreal.SystemLibrary.execute_console_command(world,'Shot showui -nosuffix filename='+str(out/'hud-after-reload.png'))
            stage('capture')
        elif phase=='capture' and time.monotonic()-at>3:
            report['status']='passed';stage('end')
    except Exception as exc:
        report.update(status='failed',error=str(exc),failed_phase=phase);stage('end')
    finally:busy=False
handle=unreal.register_slate_post_tick_callback(tick)
