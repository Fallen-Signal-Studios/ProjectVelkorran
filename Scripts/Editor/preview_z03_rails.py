"""Replace the complete sensor-route rail batch with authored full-size runs."""
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());code=(root/'Scripts/Editor/preview_z03_walls.py').read_text()
code=code.replace('Z03WallKit','Z03RailKit').replace('wall-baseline.json','rail-baseline.json').replace('PERSIST_Z03_WALLS','PERSIST_Z03_RAILS').replace('check_z03_walls','check_z03_rails').replace('z03-walls-fit.json','z03-rails-fit.json')
code=code.replace("('wall-detail',(6900,-17800,180),(10,170),80)","('landing',(7600,-17800,465),(0,90),90),('bridge',(7000,-14500,165),(0,90),90)")
exec(compile(code,'z03_rail_fit','exec'),globals())
