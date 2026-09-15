"""Approach, interior and corner-joint views of both refuge shells."""
from pathlib import Path
import unreal
code=(Path(unreal.Paths.project_dir())/'Scripts/Editor/review_eclipse_wall_scars.py').read_text();prefix=code.split('views=[',1)[0];suffix=code.split('state=dict',1)[1]
views="views=[('west-entry',(-2600,21550,-1030),(0,90),100),('west-interior',(-2600,22270,-950),(-15,90),100),('west-corner',(-2400,22600,-870),(-25,140),85),('east-interior',(3050,22330,-980),(-3,90),100),('east-entry',(3050,21550,-1030),(0,90),90)]\n"
exec(compile(prefix+views+'state=dict'+suffix,'refuge_shell_views','exec'),globals())
