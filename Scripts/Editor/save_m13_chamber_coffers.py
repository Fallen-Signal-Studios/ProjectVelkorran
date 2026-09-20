"""Save the reviewed coffer replacement, verify reload, then capture the saved map."""
from pathlib import Path
import runpy
import unreal

root=Path(unreal.Paths.project_dir())
runpy.run_path(str(root/'Scripts/Editor/preview_m13_chamber_coffers.py'),
    init_globals={'SAVE_CHAMBER_COFFERS':True})
runpy.run_path(str(root/'Scripts/Editor/preview_m13_route.py'),init_globals=dict(
    M13_ROUTE_VIEWS=[('chamber',(0,33600,-1570)),('wall-detail',(3400,37000,-1300))],
    M13_ROUTE_YAWS={'wall-detail':30}))
