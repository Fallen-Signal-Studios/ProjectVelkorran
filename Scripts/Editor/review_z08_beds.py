"""Fixed views of both evacuation trolleys and their surrounding area."""
from pathlib import Path
import unreal
code=(Path(unreal.Paths.project_dir())/'Scripts/Editor/review_eclipse_wall_scars.py').read_text();prefix=code.split('views=[',1)[0];suffix=code.split('state=dict',1)[1]
views="views=[('west-trolley',(-2510,22270,-1060),(-18,145),65),('east-trolley',(-2700,22270,-1040),(-18,35),65),('medical-overview',(-2600,22270,-950),(-35,90),110)]\n"
exec(compile(prefix+views+'state=dict'+suffix,'medical_views','exec'),globals())
