"""Fresh saved-map census and fixed wound-gallery cameras; no gameplay changes."""
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
exec(compile((root/'Scripts/Editor/audit_remaining_environment.py').read_text(),'gallery_census','exec'),globals())
capture=(root/'Scripts/Editor/review_eclipse_wall_scars.py').read_text()
prefix,suffix=capture.split('views=[',1)[0],capture.split('state=dict',1)[1]
views="views=[('entry',(-300,25750,-1330),(0,90),80),('wound',(300,28100,-1330),(0,90),80),('reverse',(350,30200,-1330),(0,-90),80)]\n"
exec(compile(prefix+views+'state=dict'+suffix,'gallery_capture','exec'),globals())
