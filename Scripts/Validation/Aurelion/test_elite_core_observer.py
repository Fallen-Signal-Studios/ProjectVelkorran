"""PIE teardown must retire diagnostics even after native objects are collected."""
import importlib.util
from pathlib import Path
import sys
from types import SimpleNamespace as NS
import unittest
from unittest.mock import Mock, patch

spec = importlib.util.spec_from_file_location('elite_observer_test',
    Path(__file__).with_name('observe_elite_core_lifecycle.py'))
module = importlib.util.module_from_spec(spec)
with patch.dict(sys.modules, {'unreal': NS()}):
    spec.loader.exec_module(module)


class EliteObserverCleanup(unittest.TestCase):
    def observer(self, world=None, actors=()):
        module.unreal = NS(UnrealEditorSubsystem=object, SovAurelionElite=object,
            get_editor_subsystem=lambda cls: NS(get_game_world=lambda: world),
            GameplayStatics=NS(get_all_actors_of_class=lambda *args: actors),
            unregister_slate_post_tick_callback=Mock())
        observer = module.Observer.__new__(module.Observer)
        observer.handle = 42
        observer.bound = {'elite'}
        observer.report = dict(errors=[], damage=[{'receipt': 'preserved'}])
        observer.write = Mock()
        return observer

    def test_collected_world_needs_no_native_wrapper_access(self):
        observer = self.observer()
        observer.stop('PIE ended')
        self.assertIsNone(observer.handle)
        self.assertFalse(observer.bound)
        self.assertEqual(observer.report['errors'], [])
        self.assertEqual(observer.report['damage'], [{'receipt': 'preserved'}])
        observer.stop('Repeated cleanup')
        module.unreal.unregister_slate_post_tick_callback.assert_called_once_with(42)

    def test_live_delegates_are_reacquired_and_detached(self):
        damage, broken = Mock(), Mock()
        actor = NS(get_path_name=lambda: 'elite',
            get_narrative_ability_system_component=lambda: NS(on_damage_resolved_as_target=damage),
            get_core_weak_points=lambda: NS(on_weak_point_broken=broken))
        observer = self.observer(object(), [actor])
        observer.stop('Stopped')
        damage.remove_callable.assert_called_once_with(observer.damage)
        broken.remove_callable.assert_called_once_with(observer.broken)

    def test_detachment_failure_cannot_keep_tick_alive(self):
        damage = NS(remove_callable=Mock(side_effect=RuntimeError('Collected during cleanup')))
        broken = Mock()
        actor = NS(get_path_name=lambda: 'elite',
            get_narrative_ability_system_component=lambda: NS(on_damage_resolved_as_target=damage),
            get_core_weak_points=lambda: NS(on_weak_point_broken=broken))
        observer = self.observer(object(), [actor])
        observer.stop('Stopped')
        self.assertIsNone(observer.handle)
        self.assertTrue(observer.report['stopped'])
        self.assertEqual(len(observer.report['errors']), 1)
        broken.remove_callable.assert_called_once()
        observer.write.assert_called_once()


if __name__ == '__main__':
    unittest.main()
