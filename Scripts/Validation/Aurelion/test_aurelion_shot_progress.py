import unittest
from aurelion_shot_progress import record_shot_progress


class ShotProgressTests(unittest.TestCase):
    def test_recorded_e2_fight_does_not_count_successful_flanks_as_failures(self):
        # HUDRouteApproach-20260919-124545-17ab8d24: (consumed rounds, matching
        # native damage transactions), in original order. Four enemies died.
        outcomes = [(1,1),(1,1),(1,1),(1,1),(0,0),(1,0),(1,1),(1,1),
                    (1,0),(0,0),(1,0),(1,1),(1,1),(1,0),(0,0),(1,0),
                    (1,0),(1,1),(1,0),(0,0),(1,0),(1,1),(1,1),(1,1),(1,0),(1,0)]
        control = dict(misses=0, flanks=0)
        for consumed, receipts in outcomes:
            record_shot_progress(control, consumed, bool(receipts))
        self.assertEqual(control, dict(misses=0, flanks=1))

    def test_persistent_misses_still_stop_after_three_failed_flanks(self):
        control = dict(misses=0, flanks=0)
        for _ in range(3):
            self.assertFalse(record_shot_progress(control, 1, False))
            self.assertTrue(record_shot_progress(control, 1, False))
        self.assertFalse(record_shot_progress(control, 1, False))
        with self.assertRaises(AssertionError):
            record_shot_progress(control, 1, False)

    def test_no_consumption_is_not_a_missed_round(self):
        control = dict(misses=1, flanks=2)
        for _ in range(20):
            self.assertFalse(record_shot_progress(control, 0, False))
        self.assertEqual(control, dict(misses=1, flanks=2))
        record_shot_progress(control, 0, True)
        self.assertEqual(control, dict(misses=0, flanks=0))


if __name__ == '__main__':
    unittest.main()
