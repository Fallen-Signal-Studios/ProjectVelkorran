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
    def __init__(self, output_directory=None, passive=False):
        self.passive = passive
        self.out = Path(output_directory or os.environ['SOV_AURELION_RUN_DIRECTORY']) / 'companion-after-shot.json'
        assert not self.out.exists()
        self.world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        assert self.world
        self.pc = unreal.GameplayStatics.get_player_controller(self.world, 0)
        self.pawn = self.pc.get_controlled_pawn()
        assert isinstance(self.pawn, (unreal.SovTarrikCharacter, unreal.SovSeleneCharacter)) and self.pawn.is_alive()
        companions = [a for a in unreal.GameplayStatics.get_all_actors_of_class(
            self.world, unreal.SovProtagonistCompanionCharacter)
            if a.is_alive() and not a.get_editor_property('hidden')]
        assert len(companions) == 1
        self.companion = companions[0]
        enemies = [a for a in unreal.GameplayStatics.get_all_actors_of_class(self.world, unreal.NarrativeNPCCharacter)
                   if a.get_class().get_name().startswith('BP_Aurelion') and a.is_alive()
                   and unreal.ArsenalStatics.get_attitude(self.pawn, a) == unreal.TeamAttitude.HOSTILE
                   and not a.get_editor_property('hidden') and a.get_actor_enable_collision()]
        census = [dict(actor=a.get_path_name(), distance=self.pawn.get_distance_to(a),
                       health=a.get_health(), visible=self.pc.line_of_sight_to(a)) for a in enemies]
        (self.out.parent / 'contact-target-census.json').write_text(json.dumps(census, indent=2))
        targets = sorted([a for a in enemies if self.pawn.get_distance_to(a) < 3000.
                          and self.pc.line_of_sight_to(a) and a.get_health() > 40
                          and 'Elite' not in a.get_class().get_name()], key=self.pawn.get_distance_to)
        assert targets, 'Requires a visible released non-Elite enemy; see contact-target-census.json'
        self.target = targets[0]
        assert self.target.get_health() > 40
        self.weapon = None
        firearm = 'Cinderline' if isinstance(self.pawn, unreal.SovTarrikCharacter) else 'Staccato'
        self.selector = wheel.Selector('/Game/Items/Weapons/WI_' + firearm + '.WI_' + firearm + '_C')
        self.owner = common.Run.get_input_owner(self, self.world)
        self.actions = {n: unreal.load_asset(common.ACTION_ROOT+n)
                        for n in ('IA_Look', 'IA_Attack', 'IA_AltAttack', 'IA_WeaponWheel')}
        assert all(self.actions.values())
        self.report = dict(diagnostic_only=True, route_qualification=False,
            direct_state_writes=False, player_damage=[], companion_damage=[], samples=[], errors=[],
            target=self.target.get_path_name())
        companion_mesh = next(m for m in self.companion.get_components_by_class(unreal.SkeletalMeshComponent)
                              if m.get_name() == 'CharacterMesh0')
        self.report['companion_mesh_frame'] = {key: companion_mesh.get_editor_property(key).export_text()
            for key in ('relative_location', 'relative_rotation', 'relative_scale3d')}
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
        if self.passive:
            return
        for name, value in {'IA_Look': (*look, 0.), 'IA_Attack': (fire, 0., 0.),
                            'IA_AltAttack': (aim, 0., 0.), 'IA_WeaponWheel': (wheel_hold, 0., 0.)}.items():
            self.owner.inject_input_vector_for_action(self.actions[name], unreal.Vector(*value), [], [])

    def tick(self, delta):
        try:
            now = time.monotonic()
            if self.passive and self.pc.get_controlled_pawn() != self.pawn:
                self.stop('Normal protagonist handoff ended observation'); return
            assert unreal.SystemLibrary.is_valid(self.pawn) and self.pc.get_controlled_pawn() == self.pawn
            assert self.pawn.is_alive() and self.companion.is_alive()
            if now-self.started > 100 or self.report['companion_damage']:
                self.stop('Observation ended'); return
            if not self.passive and not self.selector.done:
                held, report = self.selector.step(self.world)
                self.report['wheel'] = report
                self.input(wheel_hold=float(held))
                self.write()
                return
            if not self.passive:
                assert self.selector.report['status'] == 'passed', 'Normal wheel selection failed'
            if not self.passive and self.weapon is None:
                self.weapon = self.pawn.get_weapon()
                assert self.weapon and self.weapon.get_ammo_in_clip() > 0
                self.report['weapon'] = self.weapon.get_path_name()
                self.aim_started = now
            if self.passive:
                focus = self.companion.get_controller().get_focus_actor() if self.companion.get_controller() else None
                if isinstance(focus, unreal.NarrativeCharacter):
                    self.target = focus
            elif self.triggered is None:
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
            if now-self.last < .025:
                return
            self.last = now
            mesh = next(m for m in self.companion.get_components_by_class(unreal.SkeletalMeshComponent)
                        if m.get_name() == 'CharacterMesh0')
            anim = mesh.get_anim_instance()
            montage = anim.get_current_active_montage() if anim else None
            visual = self.companion.get_wielded_weapon_visual()
            blade_edges = []
            if visual and montage:
                weapon_mesh = visual.get_editor_property('weapon_mesh')
                for cls in self.companion.get_companion_component().get_editor_property('curated_abilities'):
                    ability = unreal.get_default_object(cls)
                    if not isinstance(ability, unreal.SovGameplayAbility_Melee):
                        continue
                    definition = ability.get_editor_property('attack_definition')
                    for node in definition.get_editor_property('nodes'):
                        if node.get_editor_property('montage') != montage:
                            continue
                        for edge_index, edge in enumerate([node] + list(node.get_editor_property('additional_segments'))):
                            start = weapon_mesh.get_socket_transform(edge.get_editor_property('start_socket'), unreal.RelativeTransformSpace.RTS_WORLD).transform_location(edge.get_editor_property('start_offset'))
                            end = weapon_mesh.get_socket_transform(edge.get_editor_property('end_socket'), unreal.RelativeTransformSpace.RTS_WORLD).transform_location(edge.get_editor_property('end_offset'))
                            blade_edges.append(dict(edge_index=edge_index, start=start.export_text(), end=end.export_text(),
                                radius=node.get_editor_property('trace_radius'),
                                startup=node.get_editor_property('startup'), active=node.get_editor_property('active'),
                                weapon_mesh=weapon_mesh.get_path_name(), mesh_transform=weapon_mesh.get_world_transform().export_text()))
            target_capsule = self.target.get_component_by_class(unreal.CapsuleComponent) if unreal.SystemLibrary.is_valid(self.target) else None
            movement = self.companion.get_component_by_class(unreal.CharacterMovementComponent)
            self.report['samples'].append(dict(elapsed=now-self.started,
                target_health=self.target.get_health() if unreal.SystemLibrary.is_valid(self.target) else None,
                distance=self.companion.get_distance_to(self.target),
                companion_position=self.companion.get_actor_location().export_text(),
                companion_rotation=self.companion.get_actor_rotation().export_text(),
                root_motion_rotation_allowed=movement.get_editor_property('allow_physics_rotation_during_anim_root_motion'),
                controller_rotation=self.companion.get_controller().get_control_rotation().export_text()
                    if self.companion.get_controller() else None,
                target_position=self.target.get_actor_location().export_text() if unreal.SystemLibrary.is_valid(self.target) else None,
                target_capsule=dict(center=target_capsule.get_world_location().export_text(),
                    radius=target_capsule.get_scaled_capsule_radius(), half_height=target_capsule.get_scaled_capsule_half_height())
                    if target_capsule else None,
                blade_edges=blade_edges,
                montage=montage.get_path_name() if montage else None,
                montage_position=anim.montage_get_position(montage) if montage else None,
                native_melee=list(unreal.SovMeleeValidationLibrary.describe_native_melee(
                    self.companion.get_narrative_ability_system_component())),
                focus=self.companion.get_controller().get_focus_actor().get_path_name()
                    if self.companion.get_controller() and self.companion.get_controller().get_focus_actor() else None,
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
        if self.handle is None:
            return
        self.input()
        for delegate, callback in self.delegates:
            delegate.remove_callable(callback)
        self.delegates.clear()
        unreal.unregister_slate_post_tick_callback(self.handle)
        self.handle = None
        self.report.update(stopped=True, reason=reason)
        self.write()
        self.world = self.pc = self.pawn = self.companion = self.target = self.weapon = self.owner = None
        self.selector = None


def start(output_directory=None, passive=False):
    global _RUN
    assert _RUN is None
    _RUN = Observer(output_directory, passive)
    _RUN.report['passive'] = passive
