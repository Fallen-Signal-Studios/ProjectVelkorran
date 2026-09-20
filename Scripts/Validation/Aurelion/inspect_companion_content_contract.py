"""Read-only companion grants and enemy protection census from saved content."""
import json
import os
from pathlib import Path
import unreal

def ref(value):
    return value.get_path_name() if value else None

report = dict(read_only=True, missions={}, weapons={}, enemies=[])
profiles_by_mission = {}
for name in ('M12_FireAndFrost', 'M13_ContraryWitness'):
    mission = unreal.load_asset('/Game/Aurelion/Data/DA_' + name)
    profiles = list(mission.get_editor_property('protagonist_companions'))
    profiles_by_mission[name] = profiles
    report['missions'][name] = [p.export_text() for p in profiles]
for name in ('Verity', 'Velkorran'):
    weapon = unreal.get_default_object(unreal.load_asset('/Game/Items/Weapons/WI_' + name).generated_class())
    abilities = []
    for cls in weapon.get_editor_property('weapon_abilities'):
        cdo = unreal.get_default_object(cls)
        row = dict(ability=ref(cls))
        for key in ('input_tag', 'default_bot_attack_range', 'bot_attack_min_range',
                    'bot_attack_max_range', 'attack_definition', 'activation_blocked_tags'):
            try:
                value = cdo.get_editor_property(key)
                row[key] = value.export_text() if hasattr(value, 'export_text') else str(value)
            except Exception:
                pass
        abilities.append(row)
    report['weapons'][name] = dict(visual=ref(weapon.get_editor_property('weapon_visual_class')), abilities=abilities)
report['primary_matches'] = []
for mission_name, profiles in profiles_by_mission.items():
    for profile in profiles:
        hero = str(profile.get_editor_property('companion_id'))
        weapon_name = {'Selene': 'Verity', 'Tarrik': 'Velkorran'}[hero]
        primary = [a['ability'] for a in report['weapons'][weapon_name]['abilities']
                   if a.get('input_tag') == '(TagName="Narrative.Input.Attack")']
        curated = [ref(cls) for cls in profile.get_editor_property('curated_companion_abilities')]
        report['primary_matches'].append(dict(mission=mission_name, hero=hero,
            matches=len(primary) == 1 and curated.count(primary[0]) == 1,
            primary=primary, curated=curated))
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.NarrativeNPCCharacter):
    if not actor.get_class().get_name().startswith('BP_Aurelion'):
        continue
    context = actor.get_component_by_class(unreal.SovResonanceTargetComponent)
    report['enemies'].append(dict(actor=ref(actor), resonance=ref(context),
        requires_player_finish=context.get_editor_property('requires_player_finish') if context else None))
(Path(os.environ['SOV_AURELION_RUN_DIRECTORY']) / 'companion-content-contract.json').write_text(json.dumps(report, indent=2))
