"""Inspect saved sign housings in earned-checkpoint PIE, without moving the pawn."""
from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).with_name('review_m13_live_frame.py')),
    init_globals={'M13_EXPECT_WAYFINDING':True,'M13_EXPECT_WAYFINDING_LIGHTING':True,'M13_REVIEW_VIEWS':[
        ('chamber-sign',(0,32200,-1510),-8),
        ('gallery-sign',(0,41500,160),0),
        ('departure-sign',(0,45900,160),0)]})
