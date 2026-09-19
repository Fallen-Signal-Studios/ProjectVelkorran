"""Controlled SIE regression for overlay clearing, coexistence and cue cancellation.

Uses real initialized Elite/Weaver visuals. Explicitly manipulates their overlay
slots and floor flags; this is not campaign, damage, or visual acceptance.
"""
import json
import os
import time
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
worlds = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert not worlds.get_game_world()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert editor.load_level('/Game/Aurelion/ArtReview/Chaos/L_Aurelion_ChaosPrototype')
classes = []
for index, role in enumerate(('Elite', 'Weaver')):
    cls = unreal.load_asset('/Game/Aurelion/Enemies/BP_Aurelion'+role).generated_class()
    classes.append(cls)
    actor = actors.spawn_actor_from_class(cls, unreal.Vector(0, index*600, 160))
    actor.set_editor_property('authored_placed_definition', unreal.load_asset('/Game/Aurelion/Enemies/NPC_Aurelion'+role))
    actor.set_actor_label('OverlayCoexistence'+role)
foreign = unreal.load_asset('/Game/Cues/OverlayEffect/MI_OverlayStatic')
assert foreign
report = dict(status='running', scope=__doc__, stages=[])
started = time.monotonic()
stage = 0
stage_at = started


def write():
    (out/'lethal-overlay-coexistence.json').write_text(json.dumps(report, indent=2))


def finish(error=None):
    report.update(status='failed' if error else 'passed', error=error)
    write()
    unreal.unregister_slate_post_tick_callback(handle)
    editor.editor_request_end_play()
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)


def tick(delta):
    global stage, stage_at
    try:
        now = time.monotonic()
        assert now-started < 100, 'Timed out at stage '+str(stage)
        world = worlds.get_game_world()
        if not world or now-stage_at < .5:
            return
        targets = [a for cls in classes for a in unreal.GameplayStatics.get_all_actors_of_class(world, cls)
                   if a.get_actor_label().startswith('OverlayCoexistence')]
        assert len(targets) == 2
        if not all(a.get_character_visual() and a.get_character_visual().get_all_meshes() for a in targets):
            return
        floors = [a.get_component_by_class(unreal.SovLethalFloorComponent) for a in targets]
        meshes = [m for a in targets for m in a.get_character_visual().get_all_meshes()]
        materials = [m.get_overlay_material() for m in meshes]

        def lattice():
            assert all(m and isinstance(m, unreal.MaterialInstanceDynamic)
                       and 'M_AurelionPhaseLattice' in m.get_editor_property('parent').get_path_name()
                       for m in materials), 'Lattice missing at stage '+str(stage)

        if stage == 0:
            for f in floors: f.set_floor_held(True)
        elif stage == 1:
            lattice()
            for m in meshes: m.set_overlay_material(None)
        elif stage == 2:
            lattice()
            for m in meshes: m.set_overlay_material(foreign)
        elif stage == 3:
            assert all(m == foreign for m in materials), 'Recovery overwrote a competing overlay'
            for m in meshes: m.set_overlay_material(None)
        elif stage == 4:
            lattice()
            for m in meshes: m.set_overlay_material(foreign)
            for f in floors: f.set_floor_held(False)
        elif stage == 5:
            assert all(m == foreign for m in materials), 'Removal erased a competing overlay'
            for m in meshes: m.set_overlay_material(None)
        elif stage == 6:
            assert not any(materials), 'Recovery survived cue removal'
            for f in floors: f.set_floor_held(True)
        elif stage == 7:
            lattice()
            for f in floors: f.set_floor_held(False)
        elif stage == 8:
            assert not any(materials), 'Owned overlay survived removal'
            report['stages'].append(dict(stage=stage, meshes=len(meshes), elapsed=now-started))
            finish()
            return
        report['stages'].append(dict(stage=stage, meshes=len(meshes), elapsed=now-started))
        stage += 1
        stage_at = now
        write()
    except Exception as exc:
        finish(str(exc))
        unreal.log_error('LETHAL_OVERLAY_COEXISTENCE '+str(exc))


write()
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle = unreal.register_slate_post_tick_callback(tick)
editor.editor_play_simulate()
