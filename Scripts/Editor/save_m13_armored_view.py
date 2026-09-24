"""Apply only the reviewed M13 armored-view preview and verify fresh reload."""
from pathlib import Path
import runpy
import unreal

root=Path(unreal.Paths.project_dir())
review=root/'Saved/Validation/Aurelion/M13ArmoredViewFinalPreview-20260923-183030-2fd7543b/armored-view-preview.json'
assert review.is_file()
runpy.run_path(str(root/'Scripts/Editor/preview_m13_armored_view.py'),init_globals={
    'SAVE_M13_ARMORED_VIEW':True,
    'REVIEWED_M13_ARMORED_VIEW':str(review)})
