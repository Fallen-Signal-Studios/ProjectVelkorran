"""Fixed landing, entry and E4 contact-face views."""
from pathlib import Path
import unreal
code=(Path(unreal.Paths.project_dir())/'Scripts/Editor/review_eclipse_wall_scars.py').read_text()
prefix=code.split('views=[',1)[0];suffix=code.split('state=dict',1)[1]
views="views=[('landing',(2200,20400,-735),(-5,-90),85),('entry',(0,18900,-1035),(5,90),90),('e4-contact-face',(1770,21830,-1020),(0,165),80)]\n"
exec(compile(prefix+views+'state=dict'+suffix,'deck_light_views','exec'),globals())
