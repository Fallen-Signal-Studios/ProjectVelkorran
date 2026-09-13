"""Passive animation/command census for the retained route; import is inert."""
import json
import os
from pathlib import Path
import time
import traceback
import unreal

_RUN = None


def ref(value):
    return value.get_path_name() if value else None


class Observer:
    def __init__(self, output):
        self.output = Path(output)
        assert not self.output.exists(), 'Preserve previous observation'
        self.started = time.monotonic()
        self.last = 0.
        self.seen_world = False
        self.report = dict(read_only=True, samples=[], errors=[], status='observing')
        self.handle = unreal.register_slate_post_tick_callback(self.tick)

    def tick(self, delta):
        now = time.monotonic()
        if now - self.last < .25:
            return
        self.last = now
        try:
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
            if now - self.started > 900 or (not world and self.seen_world):
                self.stop('PIE ended or observation time bound reached')
                return
            if not world:
                return
            self.seen_world = True
            for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.NarrativeNPCCharacter):
                companion = actor.get_component_by_class(unreal.SovCompanionComponent)
                if not companion or actor.get_editor_property('hidden') or not actor.is_alive():
                    continue
                activities = actor.get_activity_component()
                goal = activities.get_current_activity_goal() if activities else None
                controller = actor.get_controller()
                focus = controller.get_focus_actor() if controller else None
                visual = actor.get_character_visual()
                meshes = list(actor.get_components_by_class(unreal.SkeletalMeshComponent))
                if visual:
                    meshes.extend(visual.get_components_by_class(unreal.SkeletalMeshComponent))
                animations = []
                for mesh in meshes:
                    anim = mesh.get_anim_instance()
                    animations.append(dict(mesh=ref(mesh), anim=ref(anim),
                        montage=ref(anim.get_current_active_montage()) if anim else None))
                self.report['samples'].append(dict(elapsed=round(now-self.started, 3), actor=ref(actor),
                    identity=str(companion.get_editor_property('companion_id')),
                    position=actor.get_actor_location().export_text(), velocity=actor.get_velocity().export_text(),
                    weapon=ref(actor.get_weapon()), goal=ref(goal),
                    focus=ref(focus),
                    focus_position=focus.get_actor_location().export_text() if focus else None,
                    focus_distance_cm=actor.get_distance_to(focus) if focus else None,
                    focus_health=focus.get_health() if isinstance(focus, unreal.NarrativeCharacter) else None,
                    animations=animations))
            self.write()
        except Exception:
            self.report['errors'].append(traceback.format_exc())
            self.stop('Observer error')

    def write(self):
        self.output.parent.mkdir(parents=True, exist_ok=True)
        self.output.write_text(json.dumps(self.report, indent=2), encoding='utf-8')

    def stop(self, reason):
        unreal.unregister_slate_post_tick_callback(self.handle)
        self.report.update(status='stopped', reason=reason)
        self.write()


def start(filename='companion-animation.json'):
    global _RUN
    assert _RUN is None, 'Observer already started'
    _RUN = Observer(Path(os.environ['SOV_AURELION_RUN_DIRECTORY']) / filename)
    return _RUN
