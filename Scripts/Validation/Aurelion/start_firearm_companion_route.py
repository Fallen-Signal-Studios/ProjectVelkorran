"""Normal campaign route with firearm boundary and passive companion contact observations."""
from pathlib import Path
import runpy,sys
here=Path(__file__).resolve().parent
runpy.run_path(str(here/'observe_firearm_route.py'))
sys.path.insert(0,str(here))
import observe_companion_weapon_hit_path
observe_companion_weapon_hit_path.start()
runpy.run_path(str(here/'start_camera_perspective_route.py'))
