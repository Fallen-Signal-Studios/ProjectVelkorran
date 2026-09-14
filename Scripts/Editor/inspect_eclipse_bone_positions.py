"""Read current component-space hub/spine positions; no pose or actor writes."""
import json
import os
from pathlib import Path
import unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])/'eclipse-bone-positions.json'
assert not out.exists()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
rows={}
for actor in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SovNPCCharacterBase):
    name=actor.get_class().get_name()
    if name in rows or name not in {'BP_Aurelion'+r+'_C' for r in ('Linkbound','WallRunner','Weaver','Elite')}: continue
    mesh=actor.get_editor_property('mesh')
    bones=unreal.SovEclipseAnimationAuthoringLibrary.mesh_bones(mesh.get_skeletal_mesh_asset())
    rows[name]=[dict(bone=str(b),parent=str(mesh.get_parent_bone(b)),
        transform=mesh.get_socket_transform(b,unreal.RelativeTransformSpace.RTS_COMPONENT).export_text())
        for b in bones if 'Hub' in str(b) or 'Spine' in str(b)]
out.write_text(json.dumps(rows,indent=2),encoding='utf8')
