"""Passive player/companion source receipts during an ordinary PIE encounter.

This observer never requests commands, injects input, changes damage or moves actors.
It rebinds after a native checkpoint-world replacement so retries stay observable.
"""
import json
import time
import traceback
import unreal


def ref(value):
    return value.get_path_name() if value else None


class Observer:
    def __init__(self, output):
        self.output = output
        assert not output.exists(), 'Preserve earlier evidence'
        self.started = time.monotonic()
        self.last_sample = 0.
        self.last_write = 0.
        self.world = None
        self.pawn = None
        self.companion = None
        self.bindings = []
        self.report = dict(read_only=True, status='observing', worlds=[], damage=[], samples=[], errors=[])
        self.handle = unreal.register_slate_post_tick_callback(self.safe_tick)

    def write(self):
        self.output.write_text(json.dumps(self.report, indent=2), encoding='utf-8')

    def unbind(self):
        for delegate, callback in self.bindings:
            delegate.remove_callable(callback)
        self.bindings.clear()

    def bind(self, world, pawn, companion):
        self.unbind()
        self.world, self.pawn, self.companion = world, pawn, companion
        self.report['worlds'].append(dict(elapsed=round(time.monotonic()-self.started, 3),
            world=ref(world), player=ref(pawn), companion=ref(companion)))

        def received(owner):
            def callback(result):
                self.report['damage'].append(dict(elapsed=round(time.monotonic()-self.started, 3),
                    owner=owner, source=ref(result.source_actor), target=ref(result.target_actor),
                    health=result.applied_health_damage, shield=result.applied_shield_damage,
                    poise=result.applied_poise_damage, fatal=result.fatal))
            return callback

        for owner, actor in (('player', pawn), ('companion', companion)):
            callback = received(owner)
            delegate = actor.get_narrative_ability_system_component().on_damage_resolved_as_source
            delegate.add_callable(callback)
            self.bindings.append((delegate, callback))
        self.write()

    def safe_tick(self, delta):
        try:
            self.tick(delta)
        except Exception:
            self.report['errors'].append(traceback.format_exc())
            self.stop('Observer error')

    def tick(self, delta):
        now = time.monotonic()
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if not world:
            return
        controller = unreal.GameplayStatics.get_player_controller(world, 0)
        pawn = controller.get_controlled_pawn() if controller else None
        if not isinstance(pawn, unreal.SovPlayerCharacterBase) or not pawn.is_character_ready():
            return
        companions = [actor for actor in unreal.GameplayStatics.get_all_actors_of_class(
            world, unreal.SovProtagonistCompanionCharacter)
            if actor.is_alive() and not actor.get_editor_property('hidden')]
        if len(companions) != 1:
            return
        companion = companions[0]
        if world != self.world or pawn != self.pawn or companion != self.companion:
            self.bind(world, pawn, companion)
        if now-self.last_sample < .25:
            return
        self.last_sample = now
        ai = companion.get_controller()
        focus = ai.get_focus_actor() if ai else None
        mesh = next((part for part in companion.get_components_by_class(unreal.SkeletalMeshComponent)
            if part.get_name() == 'CharacterMesh0'), None)
        anim = mesh.get_anim_instance() if mesh else None
        montage = anim.get_current_active_montage() if anim else None
        camera = unreal.GameplayStatics.get_player_camera_manager(world, 0)
        eye = camera.get_camera_location() if camera else None
        capsule = companion.get_component_by_class(unreal.CapsuleComponent)
        self.report['samples'].append(dict(elapsed=round(now-self.started, 3),
            player_alive=pawn.is_alive(), player_health=pawn.get_health(),
            companion_alive=companion.is_alive(), companion_health=companion.get_health(),
            companion_position=companion.get_actor_location().export_text(),
            focus=ref(focus), focus_alive=focus.is_alive() if isinstance(focus, unreal.NarrativeCharacter) else None,
            focus_distance=companion.get_distance_to(focus) if focus else None,
            wielded_visual=ref(companion.get_wielded_weapon_visual()),
            montage=ref(montage),
            camera_distance_to_player=round((eye-pawn.get_actor_location()).length(),1) if eye else None,
            camera_distance_to_companion=round((eye-companion.get_actor_location()).length(),1) if eye else None,
            companion_capsule_camera_response=str(capsule.get_collision_response_to_channel(unreal.CollisionChannel.cast(4))) if capsule else None,
            opening_contribution=companion.get_companion_component().is_in_opening_contribution()))
        if now-self.last_write >= 1.:
            self.last_write = now
            self.write()

    def stop(self, reason):
        if self.handle is None:
            return
        unreal.unregister_slate_post_tick_callback(self.handle)
        self.handle = None
        self.unbind()
        self.report.update(status='stopped', reason=reason)
        self.write()


def start(output):
    return Observer(output)
