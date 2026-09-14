"""Cabinet front, worktop and medical-route context with settled captures."""
from pathlib import Path
import unreal
code=(Path(unreal.Paths.project_dir())/'Scripts/Editor/review_eclipse_wall_scars.py').read_text();prefix=code.split('views=[',1)[0];suffix=code.split('state=dict',1)[1]
views="views=[('sideboard-front',(-2600,22550,-1070),(-20,90),65),('sideboard-worktop',(-2400,22580,-1000),(-30,135),65),('medical-route',(-2600,22270,-950),(-20,90),100)]\n"
exec(compile(prefix+views+'state=dict'+suffix,'sideboard_views','exec'),globals())
