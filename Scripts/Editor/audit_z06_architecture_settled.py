"""Survey after HISM component bounds have settled through editor ticks."""
from pathlib import Path
import time,unreal
root=Path(unreal.Paths.project_dir());started=time.monotonic();unreal.EditorPythonScripting.set_keep_python_script_alive(True)
def start_survey(delta):
 if time.monotonic()-started<15:return
 unreal.unregister_slate_post_tick_callback(start_handle)
 try:exec(compile((root/'Scripts/Editor/audit_z06_architecture.py').read_text(),'audit_z06_architecture','exec'),globals())
 except Exception:
  unreal.EditorPythonScripting.set_keep_python_script_alive(False);raise
start_handle=unreal.register_slate_post_tick_callback(start_survey)
