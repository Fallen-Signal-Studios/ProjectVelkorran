"""Saved floor inventory and nearby collision; does not infer ownership from bounds."""
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
exec(compile((root/'Scripts/Editor/audit_remaining_environment.py').read_text(),'floor_census','exec'),globals())
code=(root/'Scripts/Editor/audit_z09_roof.py').read_text()
code=code.replace('[800,30850,-840]','[800,30850,-1499]').replace('[-800,25350,-901]','[-800,24394,-1513]').replace('z09-roof-neighbors.json','z09-floor-neighbors.json')
exec(compile(code,'floor_neighbors','exec'),globals())
