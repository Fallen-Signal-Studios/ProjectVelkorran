"""One ordinary player trigger, then passive companion observation in retained PIE.

Diagnostic only: never advances a route, alters resources or supplies damage.
"""
import json
import os
from pathlib import Path
import time
import traceback
import unreal
import continue_aurelion_e1_input as common
import aurelion_wheel_input as wheel

_RUN = None


class Observer:
    def __init__(self):
        self.out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY']) / 'companion-after-shot.json'
        assert not self.out.exists()
        self.world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        assert self.world
        self.pc = unreal.GameplayStatics.get_player_controller(self.world, 0)
        self.pawn = self.pc.get_controlled_pawn()
        assert isinstance(self.pawn, unreal.SovTarrikCharacter) and self.pawn.is_alive()
        companions = [a for a in unreal.GameplayStatics.get_all_actors_of_class(
            self.world, unreal.SovProtagonistCompanionCharacter)
            if a.is_alive() and not a.get_editor_property('hidden')]
        assert len(companions) == 1
        self.companion = companions[0]
        targets = [a for a in unreal.GameplayStatics.get_all_actors_of_class(self.world, unreal.SovAurelionWallRunner)
                   if a.is_alive() and not a.get_editor_property('hidden') and a.get_actor_enable_collision()
                   and self.pawn.get_distance_to(a) < 1000.]
        assert len(targets) == 1, 'Requires one nearby released WallRunner'
        self.target = targets[0]
        assert self.target.get_health() > 40
        self.weapon = None
        self.selector = wheel.Selector('/Game/Items/Weapons/WI_Cinderline.WI_Cinderline_C')
        self.owner = common.Run.get_input_owner(self, self.world)
        self.actions = {n: unreal.load_asset(common.ACTION_ROOT+n)
                        for n in ('IA_Look', 'IA_Attack', 'IA_AltAttack', 'IA_WeaponWheel')}
        assert all(self.actions.values())
        self.report = dict(diagnostic_only=True, route_qualification=False,
            direct_state_writes=False, player_damage=[], companion_damage=[], samples=[], errors=[],
            target=self.target.get_path_name())
        self.started = time.monotonic()
        self.aim_started = None
        self.triggered = None
        self.last = 0.
        self.delegates = []
        for actor, callback in ((self.pawn, self.player_damage), (self.companion, self.companion_damage)):
            delegate = actor.get_narrative_ability_system_component().on_damage_resolved_as_source
            delegate.add_callable(callback)
            self.delegates.append((delegate, callback))
        self.handle = unreal.register_slate_post_tick_callback(self.tick)

    def player_damage(self, result):
        self.report['player_damage'].append(result.export_text())

    def companion_damage(self, result):
        self.report['companion_damage'].append(result.export_text())

    def input(self, look=(0., 0.), fire=0., aim=0., wheel_hold=0.):
        for name, value in {'IA_Look': (*look, 0.), 'IA_Attack': (fire, 0., 0.),
                            'IA_AltAttack': (aim, 0., 0.), 'IA_WeaponWheel': (wheel_hold, 0., 0.)}.items():
            self.owner.inject_input_vector_for_action(self.actions[name], unreal.Vector(*value), [], [])

    def tick(self, delta):
        try:
            now = time.monotonic()
            assert unreal.SystemLibrary.is_valid(self.pawn) and self.pc.get_controlled_pawn() == self.pawn
            assert self.pawn.is_alive() and self.companion.is_alive()
            if now-self.started > 100 or self.report['companion_damage']:
                self.stop('Observation ended'); return
            if not self.selector.done:
                held, report = self.selector.step(self.world)
                self.report['wheel'] = report
                self.input(wheel_hold=float(held))
                self.write()
                return
            assert self.selector.report['status'] == 'passed', 'Normal wheel selection failed'
            if self.weapon is None:
                self.weapon = self.pawn.get_weapon()
                assert self.weapon and self.weapon.get_ammo_in_clip() > 0
                self.report['weapon'] = self.weapon.get_path_name()
                self.aim_started = now
            if self.triggered is None:
                assert now-self.aim_started < 20, 'Aim deadline; no trigger supplied'
                assert self.target.is_alive() and self.pc.line_of_sight_to(self.target)
                look, error = common.Run.look(self, self.world, self.pc, self.target.get_actor_location())
                fire = float(error < 1.5)
                if fire:
                    self.triggered = now
                    self.report['trigger_elapsed'] = now-self.started
                self.input(look, fire, 1.)
            else:
                self.input(fire=float(now-self.triggered < .05 and not self.report['player_damage']))
            if now-self.last < .1:
                return
            self.last = now
            mesh = next(m for m in self.companion.get_components_by_class(unreal.SkeletalMeshComponent)
                        if m.get_name() == 'CharacterMesh0')
            anim = mesh.get_anim_instance()
            montage = anim.get_current_active_montage() if anim else None
            visual = self.companion.get_wielded_weapon_visual()
            self.report['samples'].append(dict(elapsed=now-self.started,
                target_health=self.target.get_health() if unreal.SystemLibrary.is_valid(self.target) else None,
                distance=self.companion.get_distance_to(self.target),
                companion_position=self.companion.get_actor_location().export_text(),
                montage=montage.get_path_name() if montage else None,
                weapon_location=visual.get_actor_location().export_text() if visual else None,
                capsules=[dict(name=c.get_name(), location=c.get_world_location().export_text(),
                    radius=c.get_scaled_capsule_radius(), half_height=c.get_scaled_capsule_half_height())
                    for c in visual.get_components_by_class(unreal.CapsuleComponent)] if visual else []))
            self.write()
        except Exception:
            self.report['errors'].append(traceback.format_exc())
            self.stop('Diagnostic error')

    def write(self):
        self.out.write_text(json.dumps(self.report, indent=2), encoding='utf-8')

    def stop(self, reason):
        self.input()
        for delegate, callback in self.delegates:
            delegate.remove_callable(callback)
        self.delegates.clear()
        unreal.unregister_slate_post_tick_callback(self.handle)
        self.handle = None
        self.report.update(stopped=True, reason=reason)
        self.write()


def start():
    global _RUN
    assert _RUN is None
    _RUN = Observer()
