"""Read-only M12 E1 player capsule and authored cover-height census."""
from pathlib import Path
import hashlib
import json
import os
import unreal

root=Path(unreal.Paths.project_dir())
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
mapfile=root/'Content/Aurelion/Maps/L_Aurelion_M12.umap'
before=hashlib.sha256(mapfile.read_bytes()).hexdigest()
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Aurelion_M12' and not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
bp=unreal.load_asset('/Game/PlayerCharacters/BP_SovTarrik')
assert bp
cdo=unreal.get_default_object(bp.generated_class())
capsule=cdo.get_component_by_class(unreal.CapsuleComponent)
assert capsule
half_height=capsule.get_scaled_capsule_half_height()
assert 60<half_height<140
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
baseline=json.loads((root/'Art/Source/Aurelion/CoverKit/placement-baseline.json').read_text())
cover_names={row['path'].rsplit('.',1)[-1] for row in baseline}
assert len(cover_names)==12
rows=[]
for actor in actors:
    if actor.get_name() not in cover_names:
        continue
    component=actor.get_component_by_class(unreal.StaticMeshComponent)
    center,extent=actor.get_actor_bounds(False)
    rows.append(dict(name=actor.get_name(),label=actor.get_actor_label(),
                     mesh=component.static_mesh.get_path_name() if component and component.static_mesh else None,
                     location=actor.get_actor_location().export_text(),
                     top_z=center.z+extent.z,bottom_z=center.z-extent.z,
                     collision=str(component.get_collision_enabled()) if component else None))
assert len(rows)==12
assert hashlib.sha256(mapfile.read_bytes()).hexdigest()==before
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out/'e1-cover-heights.json').write_text(json.dumps(dict(status='read_only',map_sha256=before,
    capsule_half_height_cm=half_height,capsule_radius_cm=capsule.get_scaled_capsule_radius(),
    pilot_old_torso_offset_cm=50,pilot_old_head_offset_cm=150,
    old_head_offset_exceeds_capsule=150>half_height,
    cover=sorted(rows,key=lambda r:r['name'])),indent=2))
print('E1_COVER_HEIGHTS_AUDIT_PASS')
