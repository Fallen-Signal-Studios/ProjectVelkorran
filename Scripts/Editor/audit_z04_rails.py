"""Record the saved relay rail batch without changing the map."""
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
code=(root/'Scripts/Editor/audit_z04_floors.py').read_text().replace('Aurelion_Art_M12_Z04_16_543730','Aurelion_Art_M12_Z04_42_658d51')
code=code.replace('186','35').replace('floor-baseline.json','rail-baseline.json').replace('Z04_FLOOR_BASELINE_PASS','Z04_RAIL_BASELINE_PASS')
exec(compile(code,'audit_z04_rails','exec'),globals())
