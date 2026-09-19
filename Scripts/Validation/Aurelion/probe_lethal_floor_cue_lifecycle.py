"""Isolated simulation of cue lifecycle. No campaign or damage qualification."""
import json, os, time, unreal
from pathlib import Path
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
worlds = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert editor.load_level('/Game/Aurelion/ArtReview/Chaos/L_Aurelion_ChaosPrototype')
classes = [unreal.load_asset('/Game/Aurelion/Enemies/BP_Aurelion'+r).generated_class() for r in ('Elite','Weaver')]
for i, cls in enumerate(classes):
    actor = actors.spawn_actor_from_class(cls, unreal.Vector(0, i * 600, 160))
    assert actor
    actor.set_editor_property('authored_placed_definition', unreal.load_asset('/Game/Aurelion/Enemies/NPC_Aurelion'+('Elite','Weaver')[i]))
    actor.set_actor_label('LethalCueProbe'+str(i))
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state = dict(start=time.monotonic(), stage=0, at=0, cycles=0, rows=[])
def meshes(actor):
    visual = actor.get_character_visual()
    assert visual, actor.get_path_name() + ': missing character visual'
    return visual.get_all_meshes()
def finish():
    unreal.unregister_slate_post_tick_callback(handle)
    editor.editor_request_end_play()
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
def tick(delta):
    try:
        assert time.monotonic()-state['start'] < 90, 'Timed out'
        game = worlds.get_game_world()
        if not game: return
        now = unreal.GameplayStatics.get_time_seconds(game)
        if now < 4 or now-state['at'] < 2: return
        targets = [a for cls in classes for a in unreal.GameplayStatics.get_all_actors_of_class(game, cls)
                   if a.get_actor_label().startswith('LethalCueProbe')]
        assert len(targets) == 2
        floors = [a.get_component_by_class(unreal.SovLethalFloorComponent) for a in targets]
        assert all(floors)
        if state['stage'] == 0:
            if not all(a.get_character_visual() and a.get_character_visual().get_all_meshes() for a in targets): return
            for f in floors: f.set_floor_held(True)
            state.update(stage=1, at=now)
        elif state['stage'] == 1:
            for a, f in zip(targets, floors):
                assert f.is_floor_held()
                overlays = [m.get_overlay_material() for m in meshes(a)]
                active = [m for m in overlays if m]
                assert active, a.get_path_name() + ': no active overlay'
                assert any('M_AurelionPhaseLattice' in m.get_editor_property('parent').get_path_name() for m in active)
                state['rows'].append(dict(actor=a.get_actor_label(), cycle=state['cycles'], active_meshes=len(active)))
                f.set_floor_held(False)
            state.update(stage=2, at=now)
        else:
            for a, f in zip(targets, floors):
                assert not f.is_floor_held()
                assert not any(m.get_overlay_material() for m in meshes(a)), a.get_path_name()+': overlay survived removal'
            state['cycles'] += 1
            if state['cycles'] < 2:
                state.update(stage=0, at=now)
                return
            (out/'lethal-cue-runtime.json').write_text(json.dumps(dict(status='isolated_lifecycle_pass', rows=state['rows'],
                qualification='Two activation/removal cycles per enemy class. No visual, damage, poise or campaign phase qualification.'), indent=2))
            unreal.log('LETHAL_CUE_ISOLATED_LIFECYCLE_PASS')
            finish()
    except Exception:
        finish()
        raise
handle = unreal.register_slate_post_tick_callback(tick)
editor.editor_play_simulate()
