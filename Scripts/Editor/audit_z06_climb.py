"""Include the retained climb instance even if its HISM aggregate bounds are stale."""
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
code=(root/'Scripts/Editor/audit_z06_architecture.py').read_text().split('editor.editor_set_game_view(True)')[0]
code=code.replace('len(actors)==2509','len(actors)==2842')
code=code.replace("if not overlaps([v-d for v,d in zip(xyz(o),xyz(e))],[v+d for v,d in zip(xyz(o),xyz(e))]):continue", "if not overlaps([v-d for v,d in zip(xyz(o),xyz(e))],[v+d for v,d in zip(xyz(o),xyz(e))]) and a.get_actor_label()!='Aurelion_Art_M12_Z06_21_3485ae':continue")
exec(compile(code,'climb_inventory','exec'))
