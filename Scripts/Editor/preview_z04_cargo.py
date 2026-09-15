"""Fit complete relay stores on the existing cargo art actor."""
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());code=(root/'Scripts/Editor/preview_z03_walls.py').read_text()
code=code.replace('Z03WallKit','Z04CargoKit').replace('wall-baseline.json','cargo-baseline.json').replace('PERSIST_Z03_WALLS','PERSIST_Z04_CARGO').replace('check_z03_walls','check_z04_cargo').replace('z03-walls-fit.json','z04-cargo-fit.json')
code=code.replace("('approach',(6800,-19300,165),(0,90),90),('middle',(6800,-17200,165),(15,90),90),('wall-detail',(6900,-17800,180),(10,170),80)",
    "('approach',(7100,-12700,165),(0,90),90),('stores-detail',(6100,-11750,175),(-2,45),75),('balcony',(9300,-10600,465),(-8,135),85),('foot-cover',(8750,-11200,170),(0,135),80)")
exec(compile(code,'z04_cargo_fit','exec'),globals())
