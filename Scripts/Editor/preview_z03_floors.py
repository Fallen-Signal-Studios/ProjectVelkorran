"""Fit the complete room, ramp and bridge floor batch."""
from pathlib import Path
import unreal
import runpy
root=Path(unreal.Paths.project_dir());code=(root/'Scripts/Editor/preview_z03_walls.py').read_text()
if not globals().get('PERSIST_Z03_FLOORS',False):
    runpy.run_path(str(root/'Scripts/Editor/prepare_z03_floor_material.py'))['prepare_z03_floor_material']()
code=code.replace("'PavingIvory'","'Z03FloorStone'")
code=code.replace('Z03WallKit','Z03FloorKit').replace('wall-baseline.json','floor-baseline.json').replace('PERSIST_Z03_WALLS','PERSIST_Z03_FLOORS').replace('check_z03_walls','check_z03_floors').replace('z03-walls-fit.json','z03-floors-fit.json')
code=code.replace("('wall-detail',(6900,-17800,180),(10,170),80)","('landing',(7600,-17800,465),(-12,90),90),('underside',(7150,-18200,90),(12,35),85),('bridge',(7000,-14500,165),(-10,90),90)")
exec(compile(code,'z03_floor_fit','exec'),globals())
