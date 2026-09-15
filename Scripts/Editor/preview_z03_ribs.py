"""Fit all Z03 rib visuals using the established modular replacement workflow."""
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
code=(root/'Scripts/Editor/preview_z03_walls.py').read_text()
code=code.replace('Z03WallKit','Z03RibKit').replace('wall-baseline.json','rib-baseline.json').replace('PERSIST_Z03_WALLS','PERSIST_Z03_RIBS').replace('check_z03_walls','check_z03_ribs').replace('z03-walls-fit.json','z03-ribs-fit.json')
code=code.replace("('wall-detail',(6900,-17800,180),(10,170),80)","('rib-detail',(7050,-17800,180),(8,130),85)")
exec(compile(code,'z03_rib_fit','exec'),globals())
