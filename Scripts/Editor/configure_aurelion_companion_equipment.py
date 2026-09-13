"""Restore identity-specific, ammunition-free companion equipment in stopped editor.

Does not grant progression choices, change player inventories, or bypass native AI.
Weapon draw, attack selection and damage still require runtime qualification.
"""
import json
from pathlib import Path
import unreal


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


def main():
    assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(), 'Stop PIE first'
    result = {}
    for hero in ('Tarrik', 'Selene'):
        npc = unreal.load_asset('/Game/Aurelion/Characters/NPC_Aurelion' + hero + 'Companion')
        assert npc, hero
        result[hero] = configure(hero, npc)
        assert unreal.EditorAssetLibrary.save_loaded_asset(npc, only_if_is_dirty=False)
    path = Path('F:/ProjectVelkorran/Saved/Validation/Aurelion/CompanionCombat-20260913/equipment.json')
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(result, indent=2), encoding='utf-8')
    unreal.log('Saved companion equipment; native draw/attack validation remains pending')


if __name__ == '__main__':
    main()
