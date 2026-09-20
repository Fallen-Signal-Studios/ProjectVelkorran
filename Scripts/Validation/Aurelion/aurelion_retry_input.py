"""Ordinary-input adapter for an authored failed encounter's retry interaction.

Call step once per frame after a genuine checkpoint load has completed. This
adapter never calls StartEncounter/RetryEncounter or changes campaign state.
"""
import math
import time
import unreal
import continue_aurelion_e1_input as common


class RetryInput:
    def __init__(self, world, director, output_directory):
        matches = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovAurelionRequestActor)
                   if a.operation == unreal.SovAurelionRequest.RETRY_ENCOUNTER and a.retry_director == director]
        assert len(matches) == 1, 'Missing or duplicate authored retry interaction'
        self.actor, self.director = matches[0], director
        self.driver = common.Run(output_directory)
        self.driver.world, self.driver.owner = world, self.driver.get_input_owner(world)
        self.started = time.monotonic()
        self.samples = []
        self.last_sample = 0.
        self.hold_started = None

    def stop(self):
        self.driver.inject()
        self.driver.release_world_references()

    def step(self, world, pc, pawn):
        assert world == self.driver.world, 'Retry input cannot cross worlds'
        assert time.monotonic()-self.started < 90, 'Authored retry interaction timed out'
        assert pawn.is_alive() and pawn.is_character_ready(), 'Retry needs the ready living player'
        if self.director.get_encounter_state() == unreal.SovEncounterState.ACTIVE:
            self.driver.inject()
            assert self.director.has_encounter_player(pawn)
            assert self.driver.report['input_frames'].get('IA_Interact', 0) > 0, 'Encounter activated without this retry input'
            return True
        if self.director.get_encounter_state() == unreal.SovEncounterState.RESTORING:
            self.driver.inject()
            return False
        assert self.director.get_encounter_state() == unreal.SovEncounterState.FAILED
        actor, driver = self.actor, self.driver
        component, interaction = actor.interactable, pc.get_interaction_component()
        if self.hold_started is not None:
            # CanInteract may reject a *new* request while this admitted hold
            # owns Interacting. Keep the existing input down until native retry
            # begins; rechecking admission here prematurely cancels the hold.
            assert time.monotonic()-self.hold_started < 10., 'Admitted retry hold did not begin native restore'
            look, _ = driver.look(world, pc, actor.get_actor_location())
            driver.inject(look=look, interact=1.)
            return False
        admission = component.can_interact(pawn, interaction)
        focus = interaction.get_editor_property('viewed_interactable')
        now = time.monotonic()
        if now-self.last_sample >= 1:
            self.last_sample = now
            self.samples.append(dict(admitted=admission is not None,
                action=str(component.get_interactable_action_text(pawn, interaction)),
                focus=common._path(focus), player=pawn.get_actor_location().export_text()))
        if driver.phase == 'walk_route':
            driver.walk_route(pc, pawn)
            return False
        target, position = actor.get_actor_location(), pawn.get_actor_location()
        distance = math.hypot(position.x-target.x, position.y-target.y)
        reach = float(component.get_editor_property('interaction_distance'))
        assert math.isfinite(reach) and reach > 0
        if distance > reach*.75:
            driver.start_route([(target.x+(position.x-target.x)/distance*reach*.5,
                                 target.y+(position.y-target.y)/distance*reach*.5)], 'retry')
            return False
        look, error = driver.look(world, pc, target)
        press = error < 3 and admission is not None and focus == component
        if press:
            self.hold_started = time.monotonic()
        driver.inject(look=look, interact=1. if press else 0.)
        return False
