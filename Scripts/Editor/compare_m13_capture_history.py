"""Check M13's scene using the documented 64-frame capture preparation."""
import runpy,unreal
from pathlib import Path
runpy.run_path(str(Path(unreal.Paths.project_dir())/'Scripts/Editor/inspect_m13_shadow_quality.py'),init_globals={
    'SHADOW_COMPARISON_CASES':[('prepared-64',{'r.HighResScreenshotDelay':64}),('return-4',{'r.HighResScreenshotDelay':4})]})
