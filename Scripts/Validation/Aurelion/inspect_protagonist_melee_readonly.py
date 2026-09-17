"""Read-only inventory of the protagonists' current melee kit, for authoring native melee definitions.

Records each melee weapon's granted abilities, its visual's trace mesh and sockets, the combo
abilities' anim sets, and every montage's length, sections and notifies (the Narrative combo
opens its damage window from GameplayEvent.Attack.Activate/Deactivate notifies). Nothing is saved.
A weapon visual is spawned into the editor world only to read its component meshes, then destroyed.
"""
import json
import os
from pathlib import Path
import unreal

RUN = Path(os.environ.get('SOV_AURELION_RUN_DIRECTORY', unreal.Paths.project_saved_dir()))
OUT = RUN / 'protagonist-melee-inventory.json'
WEAPONS = {
    'Tarrik': '/Game/Items/Weapons/WI_Velkorran.WI_Velkorran_C',
    'Selene': '/Game/Items/Weapons/WI_Verity.WI_Verity_C',
}
CHARACTERS = {
    'Tarrik': '/Game/PlayerCharacters/BP_SovTarrik.BP_SovTarrik_C',
    'Selene': '/Game/PlayerCharacters/BP_SovSelene.BP_SovSelene_C',
}
EXTRA_ABILITIES = ['/Game/Abilities/Common/GA_Evade.GA_Evade_C']
report = {'read_only': True, 'weapons': {}, 'characters': {}, 'montages': {}, 'anim_sets': {},
          'abilities': {}, 'errors': []}


def path(obj):
    try:
        return obj.get_path_name() if obj is not None else None
    except Exception:
        return str(obj)


def prop(obj, name):
    try:
        return obj.get_editor_property(name)
    except Exception as exc:
        return '<unavailable: %s>' % type(exc).__name__


def text(value):
    try:
        return value.export_text()
    except Exception:
        return str(value)


def sockets_of(mesh_asset):
    names = []
    if mesh_asset is None:
        return names
    try:
        for i in range(mesh_asset.num_sockets()):
            socket = mesh_asset.get_socket_by_index(i)
            names.append({'socket': str(socket.get_editor_property('socket_name')),
                          'bone': str(socket.get_editor_property('bone_name'))})
    except Exception as exc:
        report['errors'].append('sockets %s: %s' % (path(mesh_asset), exc))
    return names


def inspect_montage(montage):
    key = path(montage)
    if not key or key in report['montages']:
        return key
    row = {'length': None, 'sections': [], 'notifies': [], 'segments': []}
    try:
        row['length'] = float(montage.get_play_length())
    except Exception as exc:
        row['length_error'] = str(exc)
    try:
        for i in range(montage.get_num_sections()):
            row['sections'].append(str(montage.get_section_name(i)))
    except Exception as exc:
        row['sections_error'] = str(exc)
    try:
        for event in unreal.AnimationLibrary.get_animation_notify_events(montage):
            row['notifies'].append(text(event))
    except Exception as exc:
        row['notifies_error'] = str(exc)
    try:
        for track in montage.get_editor_property('slot_anim_tracks'):
            for segment in track.get_editor_property('anim_track').get_editor_property('anim_segments'):
                reference = segment.get_editor_property('anim_reference')
                seg = {'slot': str(track.get_editor_property('slot_name')), 'text': text(segment),
                       'anim': path(reference), 'anim_notifies': []}
                if reference is not None:
                    try:
                        for event in unreal.AnimationLibrary.get_animation_notify_events(reference):
                            seg['anim_notifies'].append(text(event))
                    except Exception as exc:
                        seg['anim_notifies_error'] = str(exc)
                row['segments'].append(seg)
    except Exception as exc:
        row['segments_error'] = str(exc)
    report['montages'][key] = row
    return key


def inspect_anim_set(anim_set):
    key = path(anim_set)
    if not key or key in report['anim_sets']:
        return key
    rows = []
    try:
        for anim in anim_set.get_editor_property('character_anims'):
            m3 = m1 = None
            for name in ('montage3p', 'montage3_p', 'Montage3P'):
                try:
                    m3 = anim.get_editor_property(name); break
                except Exception:
                    pass
            for name in ('montage1p', 'montage1_p', 'Montage1P'):
                try:
                    m1 = anim.get_editor_property(name); break
                except Exception:
                    pass
            if m3 is None and m1 is None:
                rows.append({'export': text(anim)})
            rows.append({'montage3p': inspect_montage(m3) if m3 else None, 'montage1p': path(m1)})
    except Exception as exc:
        rows.append({'error': str(exc)})
    report['anim_sets'][key] = rows
    return key


def inspect_ability(cls):
    key = path(cls)
    if not key or key in report['abilities']:
        return key
    cdo = unreal.get_default_object(cls)
    row = {'parents': [], 'properties': {}}
    try:
        parent = cls
        for _ in range(12):
            parent = unreal.SystemLibrary.get_super_class(parent) if hasattr(unreal.SystemLibrary, 'get_super_class') else None
            if not parent:
                break
            row['parents'].append(path(parent))
    except Exception as exc:
        row['parents_error'] = str(exc)
    for name in ('input_tag', 'ability_tags', 'activation_owned_tags', 'activation_blocked_tags',
                 'default_attack_damage', 'heavy_attack_damage_multiplier', 'default_bot_attack_range'):
        row['properties'][name] = text(prop(cdo, name))
    for name in ('DefaultComboAnimations', 'DefaultDualWieldComboAnimations', 'DefaultShieldComboAnimations',
                 'HeavyAttackComboAnimSet', 'AttackComboAnimSet', 'ComboExpireTime'):
        value = prop(cdo, name)
        if isinstance(value, unreal.NarrativeAnimSet):
            row['properties'][name] = inspect_anim_set(value)
        else:
            row['properties'][name] = text(value)
    report['abilities'][key] = row
    return key


try:
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    for hero, class_path in WEAPONS.items():
        cls = unreal.load_class(None, class_path)
        cdo = unreal.get_default_object(cls)
        row = {'class': class_path}
        for name in ('weapon_abilities', 'mainhand_weapon_abilities', 'offhand_weapon_abilities', 'equipment_abilities'):
            value = prop(cdo, name)
            row[name] = [inspect_ability(c) for c in value] if isinstance(value, unreal.Array) else text(value)
        for name in ('weapon_hand', 'attack_damage', 'heavy_attack_damage_multiplier', 'bot_attack_range',
                     'wield_attachment_configs', 'holster_attachment_configs'):
            row[name] = text(prop(cdo, name))
        visual_soft = prop(cdo, 'weapon_visual_class')
        row['weapon_visual_class'] = text(visual_soft)
        try:
            visual_class = unreal.SystemLibrary.load_class_asset_blocking(visual_soft)
            actor = unreal.EditorLevelLibrary.spawn_actor_from_class(visual_class, unreal.Vector(0, 0, -100000))
            meshes = []
            for component in actor.get_components_by_class(unreal.SkeletalMeshComponent):
                asset = component.get_skeletal_mesh_asset()
                meshes.append({'component': component.get_name(), 'mesh': path(asset),
                               'component_sockets': [str(s) for s in component.get_all_socket_names()],
                               'socket_locations': {str(s): text(component.get_socket_transform(s, unreal.RelativeTransformSpace.RTS_COMPONENT))
                                                    for s in component.get_all_socket_names()},
                               'bounds': text(asset.get_bounds()) if asset else None,
                               'asset_sockets': sockets_of(asset)})
            for component in actor.get_components_by_class(unreal.StaticMeshComponent):
                meshes.append({'component': component.get_name(), 'static_mesh': path(component.static_mesh),
                               'component_sockets': [str(s) for s in component.get_all_socket_names()]})
            row['visual_meshes'] = meshes
            row['visual_actor_class'] = path(actor.get_class())
            actor.destroy_actor()
        except Exception as exc:
            row['visual_error'] = str(exc)
        report['weapons'][hero] = row
    for hero, class_path in CHARACTERS.items():
        try:
            cls = unreal.load_class(None, class_path)
            cdo = unreal.get_default_object(cls)
            mesh = cdo.get_editor_property('mesh')
            asset = mesh.get_skeletal_mesh_asset() if mesh else None
            report['characters'][hero] = {'mesh': path(asset), 'asset_sockets': sockets_of(asset),
                                          'attack_combos': text(prop(cdo, 'attack_combos')),
                                          'heavy_attack_combos': text(prop(cdo, 'heavy_attack_combos'))}
        except Exception as exc:
            report['characters'][hero] = {'error': str(exc)}
    for class_path in EXTRA_ABILITIES:
        try:
            inspect_ability(unreal.load_class(None, class_path))
        except Exception as exc:
            report['errors'].append('%s: %s' % (class_path, exc))
    for asset_path in ('/Game/Characters/Animation/VerityTwinBlades/Combo_VerityTwinBlades',
                       '/NarrativePro/Pro/Core/Character/Biped/Animation/Sequences/Weapon/Sword/1H/Combo_1H_Sword_Attack__Tarrik'):
        asset = unreal.load_asset(asset_path)
        if asset:
            inspect_anim_set(asset)
    report['status'] = 'completed_readonly' if not report['errors'] else 'completed_with_errors'
except Exception:
    import traceback
    report['status'] = 'failed'
    report['error'] = traceback.format_exc()
finally:
    OUT.write_text(json.dumps(report, indent=2), encoding='utf-8')
    unreal.log('PROTAGONIST_MELEE_INVENTORY ' + report['status'])
