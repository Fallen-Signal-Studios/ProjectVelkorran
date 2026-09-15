"""Fit all relay rail modules on the original art actor."""
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());code=(root/'Scripts/Editor/preview_z03_walls.py').read_text()
code=code.replace('Z03WallKit','Z04RailKit').replace('wall-baseline.json','rail-baseline.json').replace('PERSIST_Z03_WALLS','PERSIST_Z04_RAILS').replace('check_z03_walls','check_z04_rails').replace('z03-walls-fit.json','z04-rails-fit.json')
code=code.replace("('approach',(6800,-19300,165),(0,90),90),('middle',(6800,-17200,165),(15,90),90),('wall-detail',(6900,-17800,180),(10,170),80)",
    "('south-ramp',(9300,-11875,165),(8,90),85),('landing',(9300,-10600,465),(0,90),90),('corner',(9400,-9800,440),(-5,45),70),('west-ramp',(8250,-9900,465),(-8,180),85),('foot',(9400,-11400,170),(-15,0),65)")
exec(compile(code,'z04_rail_fit','exec'),globals())
