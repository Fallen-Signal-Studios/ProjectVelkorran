import unittest
from aurelion_route_arrival import reached_projected_destination as reached

class ProjectedArrivalTests(unittest.TestCase):
    def test_observed_ramp_endpoint(self):
        self.assertTrue(reached((79.5,4785.7,-207.7),(0,4800,-210),[(76,4800,-289.8)],True))
    def test_partial_path_cannot_finish(self):
        self.assertFalse(reached((79.5,4785.7,-207.7),(0,4800,-210),[(76,4800,-289.8)],False))
    def test_large_projection_cannot_skip_route(self):
        self.assertFalse(reached((180,4800,-210),(0,4800,-210),[(180,4800,-290)],True))
    def test_must_physically_reach_endpoint(self):
        self.assertFalse(reached((76,4750,-210),(0,4800,-210),[(76,4800,-290)],True))
    def test_wrong_floor_cannot_finish(self):
        self.assertFalse(reached((76,4800,90),(0,4800,-210),[(76,4800,-290)],True))
    def test_empty_and_nonfinite_paths_rejected(self):
        self.assertFalse(reached((0,0,0),(0,0,0),[],True))
        self.assertFalse(reached((0,0,0),(0,0,0),[(float('nan'),0,0)],True))

if __name__=='__main__': unittest.main()
