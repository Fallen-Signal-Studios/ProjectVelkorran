"""Inspect corrected faction arrows against restored docks and an ordinary PIE frame."""
from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).with_name('review_m13_live_frame.py')),init_globals={
    'M13_EXPECT_DEPARTURE_DIRECTIONS':True,'M13_EXPECT_WAYFINDING_LIGHTING':True,
    'M13_REVIEW_VIEWS':[('departure-sign',(0,45900,160),0)]})
