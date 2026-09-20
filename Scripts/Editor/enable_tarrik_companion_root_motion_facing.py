"""Let the companion's existing smoothed focus rotate during root-motion swings."""
import json
import os
from pathlib import Path
import shutil
import unreal

assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
path = '/Game/Aurelion/Characters/BP_AurelionTarrikCompanion'
bp = unreal.load_asset(path)
cdo = unreal.get_default_object(bp.generated_class())
move = cdo.get_component_by_class(unreal.CharacterMovementComponent)
field = 'allow_physics_rotation_during_anim_root_motion'
before = move.get_editor_property(field)
player = unreal.get_default_object(unreal.load_asset('/Game/PlayerCharacters/BP_SovTarrik').generated_class())
player_move = player.get_component_by_class(unreal.CharacterMovementComponent)
player_before = player_move.get_editor_property(field)
assert move and player_move and move != player_move
disk = Path(unreal.Paths.project_dir()) / 'Content/Aurelion/Characters/BP_AurelionTarrikCompanion.uasset'
backup = out / (disk.name + '.before')
assert not backup.exists()
shutil.copy2(disk, backup)
bp.modify()
move.modify()
move.set_editor_property(field, True)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
move = unreal.get_default_object(bp.generated_class()).get_component_by_class(unreal.CharacterMovementComponent)
assert move.get_editor_property(field)
assert player_move.get_editor_property(field) == player_before
assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out / 'root-motion-facing.json').write_text(json.dumps(dict(
    status='saved_requires_fresh_combat_validation', asset=path, before=before, after=True,
    player_setting=player_before, player_unchanged=True, attack_definition_changed=False), indent=2))
