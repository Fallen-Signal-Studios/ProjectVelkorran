"""Measured Z03 room inventory and approach views for the next complete surface pass."""
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
code=(root/'Scripts/Editor/audit_z06_architecture.py').read_text().split('editor.editor_set_game_view(True)')[0]
code=code.replace('len(actors)==2509','len(actors)==3140').replace('Z06','Z03').replace('z06','z03')
code=code.replace('if not overlaps([v-d','if not isinstance(c,unreal.InstancedStaticMeshComponent) and not overlaps([v-d')
exec(compile(code,'z03_architecture_survey','exec'),globals())
capture=(root/'Scripts/Editor/review_eclipse_wall_scars.py').read_text();prefix=capture.split('views=[',1)[0];suffix=capture.split('state=dict',1)[1]
views="views=[('z03-approach',(6800,-19300,165),(0,90),90),('z03-middle',(6800,-17200,165),(8,90),90),('z03-return',(6800,-14500,165),(8,-90),90)]\n"
if not globals().get('Z03_INVENTORY_ONLY',False):exec(compile(prefix+views+'state=dict'+suffix,'z03_surface_review','exec'),globals())
