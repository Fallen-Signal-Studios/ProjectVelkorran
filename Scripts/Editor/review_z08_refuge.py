"""Review the two bays and both sides of the physical baffles."""
from pathlib import Path
import unreal
code=(Path(unreal.Paths.project_dir())/'Scripts/Editor/review_eclipse_wall_scars.py').read_text();prefix=code.split('views=[',1)[0];suffix=code.split('state=dict',1)[1]
views="views=[('west-medical',(-2600,22270,-950),(-15,90),100),('west-baffle-front',(-2600,21680,-980),(-3,90),100),('west-baffle-back',(-2600,22450,-960),(-8,-90),100),('east-refuge',(3050,22330,-980),(-3,90),90)]\n"
exec(compile(prefix+views+'state=dict'+suffix,'refuge_views','exec'),globals())
