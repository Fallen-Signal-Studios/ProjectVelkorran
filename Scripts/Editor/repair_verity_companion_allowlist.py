"""Repair only the demonstrated Selene primary-class mismatch in both missions."""
import json
import os
from pathlib import Path
import shutil
import sys
import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from configure_aurelion_companion_equipment import replace_curated_primary

assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY']) / 'verity-companion-allowlist'
out.mkdir(exist_ok=False)
project = Path(unreal.Paths.project_dir()).resolve()
for name in ('M12_FireAndFrost', 'M13_ContraryWitness'):
    filename = 'DA_' + name + '.uasset'
    shutil.copy2(project / 'Content/Aurelion/Data' / filename, out / filename)
previous = unreal.load_asset('/NarrativePro/Pro/Demo/Items/Examples/Items/Weapons/Melee/Abilities/GA_Attack_Melee_Verity')
replacement = unreal.load_asset('/Game/Characters/Animation/VerityTwinBlades/GA_SovVerityTwinAttack')
assets, rows = replace_curated_primary('Selene', previous.generated_class(), replacement.generated_class())
assert len(assets) == 2, 'Expected the observed stale primary in both saved missions'
for asset in assets:
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out / 'repair.json').write_text(json.dumps(dict(status='saved', profiles=rows,
    live_damage_qualified=False, maps_unchanged=True), indent=2), encoding='utf-8')
unreal.log('VERITY_COMPANION_ALLOWLIST_REPAIRED; fresh handoff and damage remain unqualified')
