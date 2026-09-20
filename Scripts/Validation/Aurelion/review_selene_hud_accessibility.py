"""Earned Selene checkpoint HUD at normal/high contrast and three UI scales."""
import runpy
from pathlib import Path
runpy.run_path(str(Path(__file__).with_name('review_selene_checkpoint_hud.py')),init_globals={
    'HUD_REQUIRE_MATERIAL_CONTRAST':True,
    'HUD_REVIEW_STATES':[{'case':'restored','scale':1.,'contrast':False}]+[
        {'case':'WI_Staccato','scale':scale,'contrast':contrast}
        for contrast in (False,True) for scale in (1.,1.5,2.)]+[
            {'case':'WI_Staccato','scale':1.,'contrast':False}]})
