"""Regress the live E1 partial-shelter churn and close cover corners; no gameplay writes."""
import ast
import math
from pathlib import Path
from types import SimpleNamespace as NS
import unittest

source = Path(__file__).with_name('continue_aurelion_e1_input.py')
tree = ast.parse(source.read_text(encoding='utf-8-sig'))
run = next(n for n in tree.body if isinstance(n, ast.ClassDef) and n.name == 'Run')
run.body = [n for n in run.body if isinstance(n, ast.FunctionDef)
            and n.name in ('cover_movement', 'local_move')]
scope = dict(math=math, time=NS(monotonic=lambda: 10.),
             _xyz=lambda p: (p.x, p.y, p.z), _path=lambda p: 'enemy')
exec(compile(ast.fix_missing_locations(ast.Module(body=[run], type_ignores=[])),
             str(source), 'exec'), scope)


class Vector:
    def __init__(self, x, y, z):
        self.x, self.y, self.z = x, y, z

    def __add__(self, other):
        return Vector(self.x+other.x, self.y+other.y, self.z+other.z)


class CoverNavigation(unittest.TestCase):
    def setup_driver(self, exposed=False):
        shield = NS(is_initialized=lambda: True, get_shield=lambda: 10.,
                    get_max_shield=lambda: 100.)
        pawn = NS(get_actor_location=lambda: Vector(0., 0., 88.),
                  get_component_by_class=lambda cls: shield,
                  get_attached_actors=lambda: [], get_character_visual=lambda: None)
        enemies = [NS(get_actor_location=lambda: Vector(1000., 0., 88.),
                      get_character_visual=lambda: None),
                   NS(get_actor_location=lambda: Vector(2000., 0., 88.),
                      get_character_visual=lambda: None)]

        class Hit:
            def to_tuple(self):
                return (True, None, None, None, None, None, None, None, None, object())

        def trace(world, start, end, *args):
            return None if exposed and end.x == 2000. else Hit()

        def path(world, start, goal, *args):
            return NS(is_valid=lambda: True, is_partial=lambda: False,
                      path_points=[Vector(0., 0., 0.), Vector(goal.x, goal.y, 0.)])

        scope['unreal'] = NS(Vector=Vector, SovShieldComponent=object,
            NarrativeCharacter=type('Character', (), {}), HitResult=Hit,
            SovAurelionNavigationLibrary=NS(find_path_to_location_synchronously=path),
            SystemLibrary=NS(line_trace_single=trace),
            TraceTypeQuery=NS(TRACE_TYPE_QUERY1=1), DrawDebugTrace=NS(NONE=0))
        driver = scope['Run']()
        driver.cover_goal = None
        driver.cover_until = driver.next_cover_search = driver.started = 0.
        driver.report = dict(cover_attempts=[], cover_exposures=[])
        pc = NS(get_control_rotation=lambda: NS(yaw=0.))
        return driver, pawn, pc, enemies

    def test_partial_shelter_is_not_selected_for_recovery(self):
        driver, pawn, pc, enemies = self.setup_driver(exposed=True)
        self.assertIsNone(driver.cover_movement(None, pc, pawn, enemies, 10.))
        self.assertIsNone(driver.cover_goal)
        self.assertEqual(driver.report['cover_attempts'], [])

    def test_fully_sheltered_reachable_point_is_selected(self):
        driver, pawn, pc, enemies = self.setup_driver()
        self.assertIsNotNone(driver.cover_movement(None, pc, pawn, enemies, 10.))
        self.assertEqual(len(driver.report['cover_attempts']), 1)
        self.assertEqual(driver.report['cover_attempts'][0]['blocked_enemies'], 2)

    def test_cover_path_does_not_skip_a_nearby_corner(self):
        driver, pawn, pc, enemies = self.setup_driver()
        driver.cover_goal = [(0., 40., 0.), (-100., 40., 0.)]
        driver.cover_until = 20.
        movement = driver.cover_movement(None, pc, pawn, enemies, 10.)
        self.assertGreater(movement[0], 0.)
        self.assertAlmostEqual(movement[1], 0.)
        self.assertEqual(len(driver.cover_goal), 2)


if __name__ == '__main__':
    unittest.main()
