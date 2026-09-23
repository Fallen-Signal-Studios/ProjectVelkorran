"""Keep M12/M13 protagonist companions from collapsing the player's chase camera.

Only the Camera response of their existing capsules changes. Pawn blocking,
visibility, weapon collision, movement and combat remain as authored.
"""
import json
import os
import shutil
from pathlib import Path
import unreal

assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
channel=unreal.CollisionChannel.cast(4)
ignore=unreal.CollisionResponseType.ECR_IGNORE
block=unreal.CollisionResponseType.ECR_BLOCK
rows=[]

for name in ('BP_AurelionTarrikCompanion','BP_AurelionSeleneCompanion'):
    path='/Game/Aurelion/Characters/'+name
    bp=unreal.load_asset(path)
    assert bp
    cdo=unreal.get_default_object(bp.generated_class())
    capsule=cdo.get_component_by_class(unreal.CapsuleComponent)
    assert capsule
    before_camera=capsule.get_collision_response_to_channel(channel)
    before_pawn=capsule.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN)
    before_visibility=capsule.get_collision_response_to_channel(unreal.CollisionChannel.ECC_VISIBILITY)
    assert before_camera==block and before_pawn==block and before_visibility==ignore
    disk=Path(unreal.Paths.project_dir())/'Content/Aurelion/Characters'/(name+'.uasset')
    backup=out/(name+'.uasset.before')
    assert disk.is_file() and not backup.exists()
    shutil.copy2(disk,backup)
    bp.modify()
    capsule.modify()
    capsule.set_collision_response_to_channel(channel,ignore)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    capsule=unreal.get_default_object(bp.generated_class()).get_component_by_class(unreal.CapsuleComponent)
    assert capsule.get_collision_response_to_channel(channel)==ignore
    assert capsule.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN)==before_pawn
    assert capsule.get_collision_response_to_channel(unreal.CollisionChannel.ECC_VISIBILITY)==before_visibility
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
    rows.append(dict(asset=path,camera_before=str(before_camera),camera_after=str(ignore),
        pawn_unchanged=str(before_pawn),visibility_unchanged=str(before_visibility),backup=str(backup)))

assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out/'companion-camera-change.json').write_text(json.dumps(dict(status='saved_requires_fresh_pie',rows=rows),indent=2),encoding='utf-8')
