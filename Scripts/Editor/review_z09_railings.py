"""Fixed approach, rail detail and gallery-return views using the shared capture driver."""
from pathlib import Path
import unreal
code=(Path(unreal.Paths.project_dir())/'Scripts/Editor/review_eclipse_wall_scars.py').read_text()
prefix=code.split('views=[',1)[0];suffix=code.split('state=dict',1)[1]
views="views=[('approach-deck',(0,24250,-1330),(-5,90),80),('east-rail-detail',(-40,24350,-1340),(-12,35),60),('gallery-return',(0,25900,-1330),(0,-90),80)]\n"
exec(compile(prefix+views+'state=dict'+suffix,'z09_railing_capture','exec'),globals())
