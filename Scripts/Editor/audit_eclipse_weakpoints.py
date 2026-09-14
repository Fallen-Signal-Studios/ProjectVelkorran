"""Read actual native weak-point zones and mesh-bone compatibility in PIE."""
import json
import os
from pathlib import Path
import unreal

out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])/'eclipse-weakpoint-audit.json'
assert not out.exists(), 'Preserve existing audit'
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world
report=[]
for actor in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SovNPCCharacterBase):
    if actor.get_class().get_name() not in {'BP_Aurelion'+r+'_C' for r in ('Linkbound','WallRunner','Weaver','Elite')}: continue
    mesh=actor.get_editor_property('mesh')
    row=dict(actor=actor.get_path_name(),health=actor.get_health(),hidden=actor.get_editor_property('hidden'),
        mesh=mesh.get_skeletal_mesh_asset().get_path_name(), zones=[],
        head_bones=[str(b) for b in unreal.SovEclipseAnimationAuthoringLibrary.mesh_bones(mesh.get_skeletal_mesh_asset()) if 'head' in str(b).lower()],
        physics=mesh.get_skeletal_mesh_asset().get_editor_property('physics_asset').get_path_name())
    for component in actor.get_components_by_class(unreal.SovWeakPointComponent):
        for zone in component.get_editor_property('weak_point_zones'):
            row['zones'].append(dict(component=component.get_path_name(),initialized=component.is_initialized(),
                zone=zone.export_text(),broken=component.is_weak_point_broken(zone.zone_id),
                bones={str(b):mesh.does_socket_exist(b) for b in zone.hit_bones}))
    report.append(row)
out.write_text(json.dumps(report,indent=2),encoding='utf8')
