"""Unsaved fixed-camera Nanite/full-fallback comparison of the chamber coffer."""
import runpy,unreal
from pathlib import Path
runpy.run_path(str(Path(unreal.Paths.project_dir())/'Scripts/Editor/inspect_m13_shadow_quality.py'),init_globals={
    'EXTRA_COMPARISON_VARIABLES':['r.Nanite'],'COMPARISON_POSITION':(3400,37000,-1300),'COMPARISON_YAW':30,
    'SHADOW_COMPARISON_CASES':[('nanite',{'r.HighResScreenshotDelay':64}),
        ('full-fallback',{'r.HighResScreenshotDelay':64,'r.Nanite':0})]})
