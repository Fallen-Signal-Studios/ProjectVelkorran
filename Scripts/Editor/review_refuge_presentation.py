"""Refuge sign faces and western approach readability."""
from pathlib import Path
import unreal
code=(Path(unreal.Paths.project_dir())/'Scripts/Editor/review_eclipse_wall_scars.py').read_text();prefix=code.split('views=[',1)[0];suffix=code.split('state=dict',1)[1]
views="views=[('west-sign',(-2600,21860,-1030),(0,90),75),('east-sign',(3050,21860,-1030),(0,90),75),('west-context',(-2600,21680,-980),(-3,90),100),('west-approach',(-3300,21350,-1035),(0,60),90)]\n"
exec(compile(prefix+views+'state=dict'+suffix,'presentation_views','exec'),globals())
