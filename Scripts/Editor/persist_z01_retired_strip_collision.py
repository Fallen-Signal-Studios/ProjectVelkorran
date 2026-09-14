"""Persist NoCollision profiles on the two retired decorative floor strips."""
import json
import os
from pathlib import Path
import shutil
import unreal
root=Path(unreal.Paths.project_dir()); out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(); assert world.get_name()=='L_Aurelion_M12'
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
assert len(actors)==1914
labels={a.get_actor_label():a for a in actors}; rows=[]
for label,x in (('Z01__GoldChannel_01',-7180),('Z01__GoldChannel_02',-6820)):
    a=labels[label]; c=a.static_mesh_component; p=a.get_actor_location()
    assert abs(p.x-x)<.01 and abs(p.y+14700)<.01 and abs(p.z-4)<.01
    assert c.static_mesh.get_path_name()=='/Engine/BasicShapes/Cube.Cube'
    assert not c.get_editor_property('visible') and c.get_editor_property('hidden_in_game')
    rows.append(dict(label=label,before_collision=str(c.get_collision_enabled()),before_profile=str(c.get_collision_profile_name())))
(out/'retired-strip-before.json').write_text(json.dumps(rows,indent=2))
shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap')
for row in rows:
    a=labels[row['label']]; c=a.static_mesh_component
    a.modify(); c.modify(); c.set_collision_profile_name('NoCollision'); c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    assert str(c.get_collision_profile_name())=='NoCollision'
    row['after_profile']=str(c.get_collision_profile_name()); row['after_collision']=str(c.get_collision_enabled())
assert editor.save_current_level()
(out/'retired-strip-profile-save.json').write_text(json.dumps(dict(status='saved',changes=rows),indent=2))
