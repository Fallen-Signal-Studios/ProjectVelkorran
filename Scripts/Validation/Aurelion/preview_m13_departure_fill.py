"""Unsaved CP9 lighting comparison: portrait and original player camera, before/after two fills.

Uses an earned public checkpoint reload. Only two temporary PIE lights change;
production map, character poses, inventory, campaign proof and skin stay intact.
"""
from pathlib import Path
import runpy,time,unreal

root=Path(unreal.Paths.project_dir())
lighting=runpy.run_path(str(root/'Scripts/Editor/aurelion_departure_fill.py'))
def review(world,state,report,capture,end):
    step=state.setdefault('fill_step',0);now=time.monotonic()
    pc=unreal.GameplayStatics.get_player_controller(world,0)
    names=('portrait-before','portrait-fill','player-before','player-fill')
    if not state.get('fill_shot_pending'):
        if now-state['at']<15:return
        capture(world,names[step])
        manager=unreal.GameplayStatics.get_player_camera_manager(world,0)
        report['frames'][-1]['actual_view_location']=manager.get_camera_location().export_text()
        report['frames'][-1]['actual_view_rotation']=manager.get_camera_rotation().export_text()
        state.update(fill_shot_pending=True,at=now);return
    if now-state['at']<3:return
    state['fill_shot_pending']=False
    if step==0:
        state['fill_lights']=lighting['spawn_preview'](world)
        report['preview_lights']=[lighting['describe'](a) for a in state['fill_lights']]
        people=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.NarrativeCharacter)
                if isinstance(a,(unreal.SovPlayerCharacterBase,unreal.SovProtagonistCompanionCharacter))]
        ignored=people+[a.get_character_visual() for a in people if a.get_character_visual()]
        report['light_paths']=[]
        for light in state['fill_lights']:
            for person in people:
                value=unreal.SystemLibrary.line_trace_single(world,light.get_actor_location(),
                    person.get_actor_location()+unreal.Vector(0,0,70),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
                    True,ignored,unreal.DrawDebugTrace.NONE,True)
                hit=value if isinstance(value,unreal.HitResult) else next((v for v in value if isinstance(v,unreal.HitResult)),None) if isinstance(value,tuple) else None
                report['light_paths'].append(dict(light=light.get_actor_label(),person=person.get_path_name(),
                    blocked=bool(hit and hit.to_tuple()[0]),hit=str(hit.to_tuple()) if hit else None))
    elif step==1:
        pc.set_view_target_with_blend(state['original_view_target'],0)
        for a in state['fill_lights']:a.get_component_by_class(unreal.RectLightComponent).set_intensity(0)
    elif step==2:
        for a in state['fill_lights']:a.get_component_by_class(unreal.RectLightComponent).set_intensity(1600)
    elif step==3:
        # Let the screenshot render before removing the preview actors.
        state.update(fill_step=4,at=now);return
    state.update(fill_step=step+1,at=now)

def hook(world,state,report,capture,end):
    if state.get('fill_step')==4:
        if time.monotonic()-state['at']>3:
            for actor in state['fill_lights']:actor.destroy_actor()
            report['preview_lights_removed']=True;end()
        return
    review(world,state,report,capture,end)

runpy.run_path(str(Path(__file__).with_name('review_selene_face_streaming.py')),
    init_globals={'FACE_REVIEW_SCOPE':__doc__,'FACE_REVIEW_HOOK':hook})
