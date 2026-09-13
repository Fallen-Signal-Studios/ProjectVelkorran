"""Observer lifetime regressions with a fake Unreal surface; not gameplay proof."""
import os
from pathlib import Path
import runpy
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch


class Actor:
    def __init__(self, name):
        self.name, self.alive, self.collected = name, True, False
        self.callbacks = []
        self.asc = SimpleNamespace(on_damage_resolved_as_source=SimpleNamespace(
            add_callable=self.callbacks.append, remove_callable=self.callbacks.remove))

    def get_path_name(self):
        assert not self.collected, 'Accessed a collected native actor'
        return self.name

    def get_class(self):
        self.get_path_name()
        return SimpleNamespace(get_name=lambda: 'BP_AurelionLinkbound_C')

    def is_alive(self):
        self.get_path_name()
        return self.alive

    def is_character_pending_load(self):
        return False

    def get_narrative_ability_system_component(self):
        self.get_path_name()
        return self.asc


class ObserverLifetimeTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.actors, self.unregistered = [], []
        self.world = SimpleNamespace(get_path_name=lambda: '/Test/PIEWorld')
        fake = SimpleNamespace(
            UnrealEditorSubsystem=object, SovNPCCharacterBase=Actor,
            get_editor_subsystem=lambda _: SimpleNamespace(get_game_world=lambda: self.world),
            GameplayStatics=SimpleNamespace(get_all_actors_of_class=lambda *_: list(self.actors)),
            register_slate_post_tick_callback=lambda callback: 'handle',
            unregister_slate_post_tick_callback=self.unregistered.append)
        script = Path(__file__).resolve().parents[2] / 'Editor' / 'observe_eclipse_damage.py'
        with patch.dict('sys.modules', unreal=fake), patch.dict(os.environ,
                SOV_AURELION_RUN_DIRECTORY=self.directory.name):
            self.module = runpy.run_path(str(script))

    def tick(self):
        self.module['state']['last'] = 0.
        self.module['tick'](0.)

    def bind(self):
        actor = Actor('/Test/PIEWorld.Enemy')
        self.actors.append(actor)
        self.tick()
        self.assertEqual(len(actor.callbacks), 1)
        return actor

    def test_collected_actor_is_not_accessed_on_next_tick(self):
        actor = self.bind()
        actor.collected = True
        self.actors.clear()
        self.tick()
        self.assertFalse(self.module['state']['bindings'])
        self.assertFalse(self.module['report']['errors'])

    def test_dead_live_actor_delegate_is_detached(self):
        actor = self.bind()
        actor.alive = False
        self.tick()
        self.assertFalse(actor.callbacks)
        self.assertFalse(self.module['state']['bindings'])

    def test_finish_detaches_current_actor_and_preserves_damage(self):
        actor = self.bind()
        actor.callbacks[0](SimpleNamespace(export_text=lambda: 'native result'))
        self.module['finish']('test ended')
        self.assertFalse(actor.callbacks)
        self.assertEqual(self.unregistered, ['handle'])
        report = self.module['report']
        self.assertEqual(report['damage'][0]['result'], 'native result')
        self.assertEqual(report['status'], 'observation_complete')


if __name__ == '__main__':
    unittest.main()
