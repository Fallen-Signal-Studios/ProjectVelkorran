"""Keep the two mission companion primary attacks aligned with their weapons."""
import json
import os
from pathlib import Path
import shutil
import unreal

LEGACY = {
    'Tarrik': '/NarrativePro/Pro/Demo/Items/Examples/Items/Weapons/Melee/Abilities/GA_Attack_Melee_Sword_1H_Tarrik.GA_Attack_Melee_Sword_1H_Tarrik_C',
    'Selene': '/Game/Characters/Animation/VerityTwinBlades/GA_SovVerityTwinAttack.GA_SovVerityTwinAttack_C',
}

def align(hero, output):
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    weapon_name = {'Tarrik': 'Velkorran', 'Selene': 'Verity'}[hero]
    weapon = unreal.get_default_object(unreal.load_asset('/Game/Items/Weapons/WI_' + weapon_name).generated_class())
    primary = [cls for cls in weapon.get_editor_property('weapon_abilities')
               if str(unreal.GameplayTagLibrary.get_tag_name(unreal.get_default_object(cls).get_editor_property('input_tag'))) == 'Narrative.Input.Attack']
    assert len(primary) == 1
    replacement = primary[0]
    assert isinstance(unreal.get_default_object(replacement), unreal.SovGameplayAbility_Melee)
    definition = unreal.get_default_object(replacement).get_editor_property('attack_definition')
    assert definition and definition.validate() == ''
    rows = []
    for name in ('M12_FireAndFrost', 'M13_ContraryWitness'):
        mission = unreal.load_asset('/Game/Aurelion/Data/DA_' + name)
        profiles = list(mission.get_editor_property('protagonist_companions'))
        profile = next(p for p in profiles if str(p.get_editor_property('companion_id')) == hero)
        before = profile.export_text()
        abilities = list(profile.get_editor_property('curated_companion_abilities'))
        old = [cls for cls in abilities if cls.get_path_name() == LEGACY[hero]]
        if old:
            assert len(old) == 1 and replacement not in abilities
            source = Path(unreal.Paths.project_dir()) / ('Content/Aurelion/Data/DA_' + name + '.uasset')
            backup = output / (hero + '-' + source.name)
            assert not backup.exists(), 'Preserve prior backup'
            shutil.copy2(source, backup)
            profile.set_editor_property('curated_companion_abilities', [replacement if cls == old[0] else cls for cls in abilities])
            assert profile.export_text() == before.replace(LEGACY[hero], replacement.get_path_name())
            mission.set_editor_property('protagonist_companions', profiles)
            assert unreal.EditorAssetLibrary.save_loaded_asset(mission, only_if_is_dirty=False)
        else:
            assert abilities.count(replacement) == 1, 'Unknown companion primary; refusing to replace arbitrary grants'
        rows.append(dict(mission=mission.get_path_name(), hero=hero, changed=bool(old),
                         before=before, after=profile.export_text(), primary=replacement.get_path_name()))
    return rows

if __name__ == '__main__':
    out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
    rows = []
    for hero in LEGACY:
        rows.extend(align(hero, out / 'companion-grant-backups'))
    (out / 'companion-primary-alignment.json').write_text(json.dumps(dict(status='saved_requires_runtime_verification', changes=rows), indent=2))
