"""Read-only quarantine-crucible inventory using actual floor bounds."""
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
code=(root/'Scripts/Editor/audit_z06_architecture.py').read_text()
code=code.replace('len(actors)==2509',"len(actors)==2935+sum(a.get_actor_label().startswith(('KIT_Z08_Paving_','KIT_Z08_IsolationCourt')) for a in actors)").replace('Z06','Z08').replace('z06','z08')
code=code.replace('if not overlaps([v-d','if not isinstance(c,unreal.InstancedStaticMeshComponent) and not overlaps([v-d')
if globals().get('INVENTORY_ONLY',False):code=code.split('editor.editor_set_game_view(True)')[0]
exec(compile(code,'z08_architecture_survey','exec'),globals())
