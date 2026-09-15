"""Replace the entire relay floor batch with fitted custom modules."""
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
code=(root/'Scripts/Editor/preview_z03_walls.py').read_text()
code=code.replace('Z03WallKit','Z04FloorKit').replace('wall-baseline.json','floor-baseline.json')
code=code.replace('PERSIST_Z03_WALLS','PERSIST_Z04_FLOORS').replace('check_z03_walls','check_z04_floors').replace('z03-walls-fit.json','z04-floors-fit.json')
code=code.replace("'PavingIvory'","'Z03FloorStone'")
code=code.replace("('approach',(6800,-19300,165),(0,90),90),('middle',(6800,-17200,165),(15,90),90),('wall-detail',(6900,-17800,180),(10,170),80)",
    "('entry',(7000,-12600,165),(-8,90),90),('south-ramp',(9300,-11950,165),(4,90),85),('balcony',(9250,-9950,465),(-18,180),90),('west-ramp',(6900,-9900,165),(4,0),85),('underside',(8500,-10850,130),(12,50),80)")
exec(compile(code,'z04_floor_fit','exec'),globals())
