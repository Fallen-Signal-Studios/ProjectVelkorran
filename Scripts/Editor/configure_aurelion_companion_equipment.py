"""Restore identity-specific, ammunition-free companion equipment in stopped editor.

Does not grant progression choices, change player inventories, or bypass native AI.
Weapon draw, attack selection and damage still require runtime qualification.
"""
import json
from pathlib import Path
import unreal


def curated_combat_classes(hero):
    weapon_name = {'Tarrik': 'Velkorran', 'Selene': 'Verity'}[hero]
    weapon = unreal.get_default_object(unreal.load_asset('/Game/Items/Weapons/WI_' + weapon_name).generated_class())
    attacks = [cls for cls in weapon.get_editor_property('weapon_abilities')
               if str(unreal.GameplayTagLibrary.get_tag_name(unreal.get_default_object(cls).get_editor_property('input_tag'))) == 'Narrative.Input.Attack']
    assert len(attacks) == 1, 'Expected a single authored primary melee attack: ' + hero
    defense = unreal.load_asset('/Game/Abilities/' + hero + '/GA_' + hero + ('_Guard' if hero == 'Tarrik' else '_Deflection')).generated_class()
    punch = unreal.load_asset('/NarrativePro/Pro/Core/Abilities/GameplayAbilities/Attacks/Melee/GA_Melee_Punch_Unarmed').generated_class()
    return [defense, punch, attacks[0]]


def configure(hero, npc):
    weapon_name = {'Tarrik': 'Velkorran', 'Selene': 'Verity'}[hero]
    weapon = unreal.load_asset('/Game/Items/Weapons/WI_' + weapon_name)
    assert weapon, weapon_name
    weapon_class = weapon.generated_class()
    assert unreal.get_default_object(weapon_class).get_editor_property('required_ammo') is None
    existing = list(npc.get_editor_property('default_item_loadout'))
    item = unreal.ItemWithQuantity()
    item.set_editor_property('item', weapon_class)
    item.set_editor_property('quantity', 1)
    roll = unreal.LootTableRoll()
    roll.set_editor_property('items_to_grant', [item])
    expected = roll.export_text()
    assert not existing or (len(existing) == 1 and existing[0].export_text() == expected), \
        'Unexpected companion inventory: ' + hero
    npc.set_editor_property('default_item_loadout', [roll])
    assert npc.get_editor_property('default_item_loadout')[0].export_text() == expected
    return {'weapon': weapon_class.get_path_name(), 'quantity': 1,
            'loadout': expected, 'runtime_combat_verified': False}


def replace_curated_primary(hero, previous, replacement):
    """Migrate one existing allowlisted attack when its weapon grant is replaced."""
    assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    assert previous != replacement and replacement in curated_combat_classes(hero)
    assets, evidence = [], []
    for name in ('M12_FireAndFrost', 'M13_ContraryWitness'):
        mission = unreal.load_asset('/Game/Aurelion/Data/DA_' + name)
        assert mission, name
        profiles = list(mission.get_editor_property('protagonist_companions'))
        matched = [p for p in profiles if str(p.get_editor_property('companion_id')) == hero]
        assert len(matched) == 1, name
        profile = matched[0]
        before = profile.export_text()
        abilities = list(profile.get_editor_property('curated_companion_abilities'))
        changed = previous in abilities
        if changed:
            assert abilities.count(previous) == 1 and replacement not in abilities, name
            profile.set_editor_property('curated_companion_abilities',
                [replacement if cls == previous else cls for cls in abilities])
            assert profile.export_text() == before.replace(previous.get_path_name(), replacement.get_path_name())
            mission.set_editor_property('protagonist_companions', profiles)
            assets.append(mission)
        else:
            assert abilities.count(replacement) == 1, 'Missing authored primary allowlist: ' + name
        evidence.append(dict(mission=mission.get_path_name(), changed=changed,
                             before=before, after=profile.export_text()))
    return assets, evidence


def main():
    assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(), 'Stop PIE first'
    result = {}
    for hero in ('Tarrik', 'Selene'):
        npc = unreal.load_asset('/Game/Aurelion/Characters/NPC_Aurelion' + hero + 'Companion')
        assert npc, hero
        result[hero] = configure(hero, npc)
        assert unreal.EditorAssetLibrary.save_loaded_asset(npc, only_if_is_dirty=False)
    for mission_name in ('M12_FireAndFrost', 'M13_ContraryWitness'):
        mission = unreal.load_asset('/Game/Aurelion/Data/DA_' + mission_name)
        assert mission, mission_name
        profiles = list(mission.get_editor_property('protagonist_companions'))
        for profile in profiles:
            hero = str(profile.get_editor_property('companion_id'))
            profile.set_editor_property('curated_companion_abilities', curated_combat_classes(hero))
        mission.set_editor_property('protagonist_companions', profiles)
        assert unreal.EditorAssetLibrary.save_loaded_asset(mission, only_if_is_dirty=False)
    path = Path('F:/ProjectVelkorran/Saved/Validation/Aurelion/CompanionCombat-20260913/equipment.json')
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(result, indent=2), encoding='utf-8')
    unreal.log('Saved companion equipment; native draw/attack validation remains pending')


if __name__ == '__main__':
    main()
