"""Read-only native elite damage/Core events across the fresh route."""
import json
import os
from pathlib import Path
import time
import traceback
import unreal

_RUN = None


class Observer:
    def __init__(self):
        self.out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])/'elite-core-lifecycle.json'
        assert not self.out.exists()
        self.report = dict(read_only=True, damage=[], breaks=[], bindings=[], errors=[])
        self.bound = set()
        self.started = time.monotonic()
        self.last = 0.
        self.seen = False
        self.handle = unreal.register_slate_post_tick_callback(self.tick)

    def damage(self, result):
        self.report['damage'].append(dict(elapsed=time.monotonic()-self.started, result=result.export_text()))
        self.write()

    def broken(self, zone, result):
        self.report['breaks'].append(dict(elapsed=time.monotonic()-self.started, zone=str(zone), result=result.export_text()))
        self.write()

    def tick(self, delta):
        now = time.monotonic()
        if now-self.last < .5:
            return
        self.last = now
        try:
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
            if now-self.started > 1800 or (self.seen and not world):
                self.stop('Time bound or PIE ended'); return
            if not world:
                return
            self.seen = True
            for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovAurelionElite):
                path = actor.get_path_name()
                if path in self.bound:
                    continue
                asc = actor.get_narrative_ability_system_component()
                core = actor.get_core_weak_points()
                if not asc or not core:
                    continue
                asc.on_damage_resolved_as_target.add_callable(self.damage)
                core.on_weak_point_broken.add_callable(self.broken)
                # Native components may be collected before the next Python tick.
                # Keep only paths; reacquire delegates from the current world.
                self.bound.add(path)
                self.report['bindings'].append(dict(actor=path, elapsed=now-self.started,
                    state=core.capture_weak_point_state().export_text()))
            self.write()
        except Exception:
            self.report['errors'].append(traceback.format_exc())
            self.stop('Observer error')

    def stop(self, reason):
        if self.handle is None:
            return
        unreal.unregister_slate_post_tick_callback(self.handle)
        self.handle = None
        try:
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
            actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovAurelionElite) if world else []
            for actor in actors:
                if actor.get_path_name() not in self.bound:
                    continue
                for component, delegate, callback in (
                    (actor.get_narrative_ability_system_component(), 'on_damage_resolved_as_target', self.damage),
                    (actor.get_core_weak_points(), 'on_weak_point_broken', self.broken)):
                    try:
                        if component:
                            getattr(component, delegate).remove_callable(callback)
                    except Exception:
                        self.report['errors'].append(traceback.format_exc())
        except Exception:
            self.report['errors'].append(traceback.format_exc())
        finally:
            self.bound.clear()
            self.report.update(stopped=True, reason=reason)
            self.write()

    def write(self):
        self.out.write_text(json.dumps(self.report, indent=2), encoding='utf-8')


def start():
    global _RUN
    assert _RUN is None
    _RUN = Observer()
