"""Fixed ramp, landing and departure views using the shared capture driver."""
from pathlib import Path
import unreal
code=(Path(unreal.Paths.project_dir())/'Scripts/Editor/review_eclipse_wall_scars.py').read_text()
prefix=code.split('views=[',1)[0];suffix=code.split('state=dict',1)[1]
views="views=[('east-ramp',(1600,19400,-1035),(10,35),80),('upper-landing',(2200,20400,-735),(-5,-90),85),('departure-ramp',(0,23200,-1035),(-8,90),85)]\n"
exec(compile(prefix+views+'state=dict'+suffix,'railing_capture','exec'),globals())
