"""Regression for the observed E1 railing corner; no Unreal gameplay is simulated."""
import ast
import math
from pathlib import Path
from types import SimpleNamespace as NS
import unittest

source = Path(__file__).with_name('continue_aurelion_e1_input.py')
tree = ast.parse(source.read_text(encoding='utf-8-sig'))
run = next(n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == 'Run')
run.body = [n for n in run.body if isinstance(n, ast.FunctionDef) and n.name in ('approach', 'local_move')]
scope = dict(math=math, time=NS(monotonic=lambda: 10.), _xyz=lambda p: (p.x,p.y,p.z), _path=lambda p: 'target')
exec(compile(ast.fix_missing_locations(ast.Module(body=[run], type_ignores=[])), str(source), 'exec'), scope)

class PathCorners(unittest.TestCase):
    def drive(self, position, points, partial=False):
        vector = lambda p: NS(x=p[0], y=p[1], z=10.)
        path = NS(is_valid=lambda: True, is_partial=lambda: partial, path_points=[vector(p) for p in points])
        scope['unreal'] = NS(SovAurelionNavigationLibrary=NS(find_path_to_location_synchronously=lambda *args: path))
        driver = scope['Run']()
        driver.path_target = None
        driver.last_path = 0.
        driver.report = {}
        pawn = NS(get_actor_location=lambda: vector(position))
        target = NS(get_actor_location=lambda: vector(points[-1]))
        pc = NS(get_control_rotation=lambda: NS(yaw=0.))
        return driver.approach(None, pc, pawn, target), driver

    def test_observed_railing_corner_is_not_skipped(self):
        movement, driver = self.drive((-6354.939, -12958.270),
            [(-6340.426,-12944.574),(-6365.,-12920.),(-6422.,-12920.),(-7320.493,-13339.134)])
        # The first turn goes north. The old 80 cm tolerance skipped both
        # corners and drove south-west directly into the railing.
        self.assertGreater(movement[0], 0.)
        self.assertLess(movement[1], 0.)
        self.assertEqual(len(driver.path_points), 3)

    def test_reached_corner_advances_along_path(self):
        movement, driver = self.drive((-6365.,-12920.),
            [(-6365.,-12920.),(-6365.,-12920.),(-6422.,-12920.),(-7320.,-13339.)])
        self.assertAlmostEqual(movement[0], 0.)
        self.assertLess(movement[1], 0.)
        self.assertEqual(len(driver.path_points), 2)

    def test_partial_path_follows_reachable_prefix(self):
        movement, driver = self.drive((0.,0.), [(0.,0.),(0.,150.),(270.,150.)], partial=True)
        self.assertGreater(movement[0], 0.)
        self.assertAlmostEqual(movement[1], 0.)
        self.assertTrue(driver.report['last_combat_path']['partial'])
        self.assertFalse(driver.report['last_combat_path']['complete'])
        self.assertEqual(len(driver.path_points), 2)

    def test_partial_path_with_no_advance_stops(self):
        movement, driver = self.drive((0.,0.), [(0.,0.),(40.,0.)], partial=True)
        self.assertEqual(movement, (0.,0.))
        self.assertEqual(driver.path_points, [])

if __name__ == '__main__':
    unittest.main()
