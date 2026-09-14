"""Fixed room and close cargo inspection views."""
from pathlib import Path
import unreal
code=(Path(unreal.Paths.project_dir())/'Scripts/Editor/review_eclipse_wall_scars.py').read_text();prefix=code.split('views=[',1)[0];suffix=code.split('state=dict',1)[1]
suffix=suffix.replace('next=time.monotonic()+15','next=time.monotonic()+30',1)
views="views=[('entry',(0,18900,-1035),(5,90),90),('west-stores',(-2180,18600,-975),(-10,128),70),('stores-detail',(-2330,19100,-1040),(-8,165),65),('entry-return',(0,18900,-1035),(5,90),90)]\n"
exec(compile(prefix+views+'state=dict'+suffix,'cargo_views','exec'),globals())
