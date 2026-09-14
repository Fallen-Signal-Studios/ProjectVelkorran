"""Read-only saved Z07 architecture inventory and opposing room views."""
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
code=(root/'Scripts/Editor/audit_z06_architecture.py').read_text()
code=code.replace('len(actors)==2509',"len(actors)==2843+sum(a.get_actor_label().startswith('KIT_Z07_Paving_') for a in actors)").replace('Z06','Z07').replace('z06','z07')
# Cold HISM aggregate bounds can be zero. Test their individual instances regardless.
code=code.replace('if not overlaps([v-d', 'if not isinstance(c,unreal.InstancedStaticMeshComponent) and not overlaps([v-d')
if globals().get('INVENTORY_ONLY',False):
    code=code.split('editor.editor_set_game_view(True)')[0]
exec(compile(code,'z07_architecture_survey','exec'),globals())
