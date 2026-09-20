"""Isolate short-range ambient occlusion at the actual chamber coffer camera."""
import runpy,unreal
from pathlib import Path
names=['r.Lumen.ScreenProbeGather.ShortRangeAO','r.Lumen.DiffuseIndirect.SSAO','r.AmbientOcclusionLevels']
runpy.run_path(str(Path(unreal.Paths.project_dir())/'Scripts/Editor/inspect_m13_shadow_quality.py'),init_globals={
    'EXTRA_COMPARISON_VARIABLES':names,'COMPARISON_POSITION':(3400,37000,-1300),'COMPARISON_YAW':30,
    'SHADOW_COMPARISON_CASES':[('baseline-ao',{'r.HighResScreenshotDelay':64}),
        ('without-short-range-ao',dict({'r.HighResScreenshotDelay':64},**{name:0 for name in names}))]})
