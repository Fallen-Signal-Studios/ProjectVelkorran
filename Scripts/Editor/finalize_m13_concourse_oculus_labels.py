"""Give the two reviewed, saved Z12 oculus actors production labels."""
from pathlib import Path
import hashlib
import json
import os
import runpy
import shutil
import unreal

root=Path(unreal.Paths.project_dir())
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
map_path=root/'Content/Aurelion/Maps/L_Aurelion_M13.umap'
m12_path=root/'Content/Aurelion/Maps/L_Aurelion_M12.umap'
digest=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
before=(digest(m12_path),digest(map_path))
assert before==('64a4517bb88719694793a46ae859f0eb6fbc861eef10e956e0574d8962b844a4',
                '52a3be7f71d67f08550a37056f0053185f7065067460f748afadf34080eeda72')
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Aurelion_M13' and not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
existing=list(actor_api.get_all_level_actors())
by_label={a.get_actor_label():a for a in existing}
renames=[('PREVIEW_Z12_SovereignOculus_47200','Aurelion_Z12_Dominion_SovereignOculus'),
         ('PREVIEW_Z12_SovereignOculus_47800','Aurelion_Z12_Reformation_SovereignOculus')]
asset='/Game/Aurelion/Environment/ArchitectureKit/Meshes/SM_Aurelion_KIT_Z12SovereignOculus_6m'
helper=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
unchanged=helper['snapshot_actor_state']([a for a in existing
    if a.get_actor_label() not in [pair[0] for pair in renames]])
for old,new in renames:
    assert new not in by_label
    actor=by_label[old]
    assert actor.static_mesh_component.static_mesh.get_path_name().split('.')[0]==asset
    assert actor.static_mesh_component.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
    assert not actor.static_mesh_component.get_editor_property('can_ever_affect_navigation')
    actor.set_actor_label(new)
assert helper['snapshot_actor_state']([a for a in existing
    if a.get_actor_label() not in [pair[1] for pair in renames]])==unchanged
backup=out/'L_Aurelion_M13.before.umap'
shutil.copy2(map_path,backup)
assert digest(backup)==before[1]
assert level.save_current_level()
assert level.load_level('/Game/Aurelion/Maps/L_Aurelion_M13')
loaded={a.get_actor_label():a for a in actor_api.get_all_level_actors()}
assert all(old not in loaded and new in loaded for old,new in renames)
for old,new in renames:
    body=loaded[new].static_mesh_component
    assert body.static_mesh.get_path_name().split('.')[0]==asset
    assert body.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
    assert not body.get_editor_property('can_ever_affect_navigation')
assert helper['snapshot_actor_state']([a for a in loaded.values()
    if a.get_actor_label() not in [pair[1] for pair in renames]])==unchanged
after=(digest(m12_path),digest(map_path))
assert after[0]==before[0] and after[1]!=before[1]
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out/'oculus-label-finalization.json').write_text(json.dumps(dict(
    status='saved_reloaded',renames=renames,m12_sha256=after[0],
    m13_sha256_before=before[1],m13_sha256_after=after[1],
    other_actor_state_preserved=True,map_backup=str(backup)),indent=2))
print('Z12_OCULUS_LABELS_SAVED_RELOADED')
