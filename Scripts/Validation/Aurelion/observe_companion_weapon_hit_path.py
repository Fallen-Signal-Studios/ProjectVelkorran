"""Passive native weapon-cache evidence. Never triggers traces, attacks or damage."""
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
    def __init__(self, filename):
        self.out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY']) / filename
        assert not self.out.exists(), 'Preserve earlier evidence'
        self.report = dict(read_only=True, samples=[], errors=[], inaccessible={})
        self.started = time.monotonic()
        self.last = 0.
        self.seen = False
        self.handle = unreal.register_slate_post_tick_callback(self.safe_tick)

    def safe_tick(self, delta):
        try:
            self.tick(delta)
        except Exception:
            self.report['errors'].append(traceback.format_exc())
            self.stop('Observer error')

    def tick(self, delta):
        now = time.monotonic()
        if now - self.last < .1:
            return
        self.last = now
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if now - self.started > 1800 or (self.seen and not world):
            self.stop('Time bound reached or PIE ended')
            return
        if not world:
            return
        self.seen = True
        for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovProtagonistCompanionCharacter):
            if actor.get_editor_property('hidden') or not actor.is_alive():
                continue
            visual = actor.get_wielded_weapon_visual()
            if not visual:
                continue
            mesh = next((m for m in actor.get_components_by_class(unreal.SkeletalMeshComponent)
                         if m.get_name() == 'CharacterMesh0'), None)
            anim = mesh.get_anim_instance() if mesh else None
            montage = anim.get_current_active_montage() if anim else None
            if not montage:
                continue
            focus = actor.get_controller().get_focus_actor() if actor.get_controller() else None
            row = dict(elapsed=round(now-self.started, 3), actor=ref(actor), visual=ref(visual),
                       montage=ref(montage), focus=ref(focus),
                       distance=actor.get_distance_to(focus) if focus else None)
            for key in ('collision_data', 'cached_hit_actors', 'current_notify_event'):
                if key in self.report['inaccessible']:
                    continue
                try:
                    value = visual.get_editor_property(key)
                    row[key] = ([ref(a) for a in value] if key == 'cached_hit_actors'
                                else len(value) if key == 'collision_data' else value.export_text())
                except Exception as exc:
                    self.report['inaccessible'][key] = str(exc)
            self.report['samples'].append(row)
        self.write()

    def write(self):
        self.out.write_text(json.dumps(self.report, indent=2), encoding='utf-8')

    def stop(self, reason):
        if self.handle is not None:
            unreal.unregister_slate_post_tick_callback(self.handle)
            self.handle = None
        self.report.update(stopped=True, reason=reason)
        self.write()

def start(filename='companion-hit-path.json'):
    global _RUN
    assert _RUN is None, 'Observer already started'
    _RUN = Observer(filename)
