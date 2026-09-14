"""Wait for retained collision profiles to settle, then fit and capture the crown."""
from pathlib import Path
import time,unreal
root=Path(unreal.Paths.project_dir());started=time.monotonic()
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
def start_fit(delta):
 if time.monotonic()-started<15:return
 unreal.unregister_slate_post_tick_callback(start_handle)
 try:exec(compile((root/'Scripts/Editor/fit_atrium_crown.py').read_text(),'fit_atrium_crown','exec'),globals())
 except Exception:
  unreal.EditorPythonScripting.set_keep_python_script_alive(False);raise
start_handle=unreal.register_slate_post_tick_callback(start_fit)
