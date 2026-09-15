"""Read-only relay overlook room geometry, instances and approach views."""
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
code=(root/'Scripts/Editor/audit_z06_architecture.py').read_text().split('editor.editor_set_game_view(True)')[0]
code=code.replace('len(actors)==2509','len(actors)==3140').replace('Z06','Z04').replace('z06','z04')
code=code.replace('if not overlaps([v-d','if not isinstance(c,unreal.InstancedStaticMeshComponent) and not overlaps([v-d')
exec(compile(code,'z04_architecture_survey','exec'),globals())
capture=(root/'Scripts/Editor/review_eclipse_wall_scars.py').read_text();prefix=capture.split('views=[',1)[0];suffix=capture.split('state=dict',1)[1]
views="views=[('entry',(origin.x,origin.y-extent.y+300,165),(5,90),90),('middle',(origin.x,origin.y,165),(10,90),90),('return',(origin.x,origin.y+extent.y-300,165),(8,-90),90)]\n"
exec(compile(prefix+views+'state=dict'+suffix,'z04_room_review','exec'),globals())
