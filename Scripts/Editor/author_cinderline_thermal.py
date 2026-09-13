"""Give project Cinderline its TDD Thermal channel, preserving the existing payload graph."""
import json
import os
from pathlib import Path
import shutil
import traceback
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])/'cinderline-authoring'
out.mkdir(parents=True, exist_ok=True)
report = dict(status='running', saved=[], gameplay_qualified=False)
effect_source = '/NarrativePro/Pro/Core/Abilities/GameplayEffects/GE_WeaponDamage'
ability_source = '/NarrativePro/Pro/Core/Abilities/GameplayAbilities/Attacks/Firearms/GA_Firearm_Cinderline'
effect_path = '/Game/Abilities/Tarrik/GE_CinderlineThermal'
ability_path = '/Game/Abilities/Tarrik/GA_CinderlinePrimary'

def cdo(asset):
    return unreal.get_default_object(asset.generated_class())

def tag_component(defaults):
    rows = [c for c in defaults.get_editor_property('ge_components')
            if c.get_class().get_name() == 'AssetTagsGameplayEffectComponent']
    assert len(rows) == 1, 'Expected one existing asset-tags component'
    return rows[0]

try:
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
    weapon = unreal.load_asset('/Game/Items/Weapons/WI_Cinderline')
    source_effect = unreal.load_asset(effect_source)
    source_ability = unreal.load_asset(ability_source)
    assert weapon and source_effect and source_ability
    assert cdo(source_ability).get_editor_property('Damage Effect Class') == source_effect.generated_class()
    original_tags = tag_component(cdo(source_effect)).get_editor_property('InheritableAssetTags').export_text()
    assert 'Sov.Damage.Channel.Edge' in original_tags
    grants = {key:list(cdo(weapon).get_editor_property(key)) for key in
              ('weapon_abilities', 'mainhand_weapon_abilities', 'offhand_weapon_abilities')}
    for key in ('weapon_abilities', 'mainhand_weapon_abilities'):
        assert grants[key].count(source_ability.generated_class()) == 1, key
    assert not unreal.EditorAssetLibrary.does_asset_exist(effect_path), 'Refusing existing effect overwrite'
    assert not unreal.EditorAssetLibrary.does_asset_exist(ability_path), 'Refusing existing ability overwrite'
    report['original_tags'] = original_tags
    report['original_grants'] = {k:[a.get_path_name() for a in v] for k,v in grants.items()}
    effect = unreal.EditorAssetLibrary.duplicate_asset(effect_source, effect_path)
    component = tag_component(cdo(effect))
    changes = component.get_editor_property('InheritableAssetTags')
    assert changes.import_text(original_tags.replace('Sov.Damage.Channel.Edge', 'Sov.Damage.Channel.Thermal'))
    component.set_editor_property('InheritableAssetTags', changes)
    # Keep the legacy mirror consistent with the current component representation.
    cdo(effect).set_editor_property('inheritable_gameplay_effect_tags', changes)
    unreal.BlueprintEditorLibrary.compile_blueprint(effect)
    combined = tag_component(cdo(effect)).get_editor_property('InheritableAssetTags').get_editor_property('CombinedTags').export_text()
    assert 'Sov.Damage.Channel.Thermal' in combined and 'Sov.Damage.Channel.Edge' not in combined, combined
    ability = unreal.EditorAssetLibrary.duplicate_asset(ability_source, ability_path)
    cdo(ability).set_editor_property('Damage Effect Class', effect.generated_class())
    unreal.BlueprintEditorLibrary.compile_blueprint(ability)
    assert cdo(ability).get_editor_property('Damage Effect Class') == effect.generated_class()
    backup = out/'WI_Cinderline-before.uasset'
    assert not backup.exists(), 'Refusing backup overwrite'
    shutil.copy2(root/'Content/Items/Weapons/WI_Cinderline.uasset', backup)
    for asset in (effect, ability):
        assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
        report['saved'].append(asset.get_path_name())
    for key, values in grants.items():
        cdo(weapon).set_editor_property(key, [ability.generated_class() if a == source_ability.generated_class() else a for a in values])
    unreal.BlueprintEditorLibrary.compile_blueprint(weapon)
    for key, values in grants.items():
        assert list(cdo(weapon).get_editor_property(key)) == [ability.generated_class() if a == source_ability.generated_class() else a for a in values]
    assert unreal.EditorAssetLibrary.save_loaded_asset(weapon, only_if_is_dirty=False)
    report['saved'].append(weapon.get_path_name())
    report.update(status='authored_requires_live_damage_validation', combined_tags=combined)
except Exception:
    report.update(status='failed', error=traceback.format_exc())
finally:
    (out/'report.json').write_text(json.dumps(report, indent=2), encoding='utf8')
    unreal.log('CINDERLINE_THERMAL_AUTHORING '+report['status'])
