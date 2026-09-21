"""Hold the wider camera that exposed black face shading in earned CP9 gameplay."""
from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).with_name('probe_selene_live_color.py')),init_globals={
    'M13_COLOR_FIXED_CAMERA':{'position':(1350,48200,170),'yaw':-90,'fov':80}})
