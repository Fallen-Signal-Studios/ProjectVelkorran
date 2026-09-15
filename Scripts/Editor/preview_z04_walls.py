"""Replace the complete relay wall batch and capture interior and exterior faces."""
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());code=(root/'Scripts/Editor/preview_z03_walls.py').read_text()
code=code.replace('Z03WallKit','Z04WallKit').replace('PERSIST_Z03_WALLS','PERSIST_Z04_WALLS').replace('check_z03_walls','check_z04_walls').replace('z03-walls-fit.json','z04-walls-fit.json')
code=code.replace("('approach',(6800,-19300,165),(0,90),90),('middle',(6800,-17200,165),(15,90),90),('wall-detail',(6900,-17800,180),(10,170),80)","('frontage',(7000,-13900,200),(12,90),95),('interior',(6900,-11500,180),(8,90),90),('detail',(8300,-12000,250),(5,-90),75)")
exec(compile(code,'z04_wall_fit','exec'),globals())
