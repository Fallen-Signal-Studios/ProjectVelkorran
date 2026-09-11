"""Assign persistent identities to the exact stopped-editor Aurelion placed cast.

No imports run editor queries. No map/asset saves, runtime initialization, definition
assignment, Blueprint compilation, save-file edits, or old-save migration occur here.
The owning assembly saves the map only after this and its existing gates pass.
"""
import hashlib
import json
import re

NAMESPACE = 'SovereignCall.Origins.Aurelion.PlacedNPC.v1'
MAPS = {12: '/Game/Aurelion/Maps/L_Aurelion_M12',
        13: '/Game/Aurelion/Maps/L_Aurelion_M13'}
STAMP = 'Sov.Aurelion.WorkPC.20260907'
ZERO = '0' * 32


def identity_guid(map_package, actor_label):
    """Versioned UTF-8 key; generated UObject names and encounter ownership are absent."""
    if map_package not in MAPS.values() or not re.fullmatch(r'Aurelion_[A-Za-z0-9_.]+', actor_label):
        raise RuntimeError('Identity key requires an exact owned map and semantic actor label')
    key = NAMESPACE + '\n' + map_package + '\n' + actor_label
    value = hashlib.sha256(key.encode('utf-8')).hexdigest()[:32].upper()
    if value == ZERO:
        raise RuntimeError('Namespaced identity unexpectedly produced an invalid GUID')
    return key, value


def _digits(value):
    text = value.export_text().upper()
    if not re.fullmatch(r'[0-9A-F]{32}', text):
        raise RuntimeError('Native FGuid did not export exactly 32 hexadecimal digits: ' + text)
    return text


def _copy(value):
    result = type(value)()
    if not result.import_text(value.export_text()):
        raise RuntimeError('Detached SpawnInfo copy could not import its full source export')
    return result


def _other_metadata(info):
    """Compare every current/future SpawnInfo field except the one owned GUID."""
    value = _copy(info)
    # A detached copy can use the supported struct text importer. Live actor
    # mutation goes through the scoped native authoring helper below.
    if not value.import_text('(SpawnAssignedSaveGUID='+ZERO+')'):
        raise RuntimeError('Detached SpawnInfo normalization failed')
    if _digits(value.get_editor_property('spawn_assigned_save_guid')) != ZERO:
        raise RuntimeError('Detached SpawnInfo normalization changed the expected GUID')
    return value.export_text()


def _assert_world(context):
    import unreal
    chapter, world = context['chapter'], context['world']
    if chapter not in MAPS:
        raise RuntimeError('Identity authoring requires M12 or M13')
    if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
        raise RuntimeError('Stop PIE before authoring placed identities')
    if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world() != world:
        raise RuntimeError('Identity authoring world is no longer the current editor world')
    if world.get_path_name().split('.')[0] != MAPS[chapter]:
        raise RuntimeError('Identity authoring refuses a source or unrelated map')
    return world


def assign(context, encounter_result, evidence):
    """Populate caller-owned evidence, including failure; mutate only zero owned GUIDs.

    Must run after all placed NPCs are authored and owned GUID graphs are compiled,
    before navigation/map save. The real interface must work with NPCDefinition
    still null, which exercises the pre-BeginPlay lookup needed during native load.
    """
    import unreal
    from encounter_content import ROSTERS
    from cinematic_content import CAST
    evidence.update(schema_version=1, namespace=NAMESPACE, status='checking',
        algorithm='SHA256(UTF8(namespace + LF + map_package + LF + actor_label))[0:16], uppercase FGuid digits',
        map=MAPS.get(context['chapter']), expected_count=34 if context['chapter'] == 12 else 0,
        checked=0, rows=[], failures=[], runtime_reload_verified=False)
    try:
        world = _assert_world(context)
        chapter, map_package = context['chapter'], MAPS[context['chapter']]
        hostiles, cast = encounter_result['hostiles'], context['cast']
        expected_hostiles = {group+'.'+row[0] for group, rows in ROSTERS.items() for row in rows} if chapter == 12 else set()
        expected_cast = set(CAST) if chapter == 12 else set()
        if set(hostiles) != expected_hostiles or set(cast) != expected_cast:
            raise RuntimeError('Exact authored hostile/canonical story identities are incomplete')
        if len(expected_hostiles) != (24 if chapter == 12 else 0) or len(expected_cast) != (10 if chapter == 12 else 0):
            raise RuntimeError('Reviewed 24 hostile / 10 canonical story budget changed')
        targets = [('encounter', key, 'Aurelion_'+key, hostiles[key]) for key in sorted(hostiles)]
        targets += [('story', key, 'Aurelion_Cast_'+key, cast[key]) for key in sorted(cast)]
        all_npcs = [a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
                    if isinstance(a, unreal.NarrativeNPCCharacter) and a.get_world() == world]
        selected_paths = [a.get_path_name() for _, _, _, a in targets]
        if len(set(selected_paths)) != len(targets):
            raise RuntimeError('Two canonical identities alias the same placed actor')
        # Only exact creation references may be changed; a label alone is not ownership.
        owned_paths = {a.get_path_name() for a in all_npcs if unreal.Name(STAMP) in a.tags
                       and a.get_class().get_path_name().startswith('/Game/Aurelion/')}
        if owned_paths != set(selected_paths):
            raise RuntimeError('Owned placed NPC census differs from explicit authoring references')
        untouched = [(a, a.get_editor_property('spawn_info').export_text()) for a in all_npcs
                     if a.get_path_name() not in owned_paths]
        planned, guids = [], set()
        # Preflight the complete batch before publishing any GUID. Supplied/spawner
        # identities are never overwritten, including on an idempotent rerun.
        for kind, semantic, label, actor in targets:
            if not unreal.SystemLibrary.is_valid(actor) or actor.get_world() != world or not isinstance(actor, unreal.SovNPCCharacterBase):
                raise RuntimeError('Placed identity has an invalid current native owner: '+label)
            if actor.get_actor_label() != label or unreal.Name(STAMP) not in actor.tags:
                raise RuntimeError('Placed identity label/authoring ownership mismatch: '+label)
            definition = actor.get_editor_property('authored_placed_definition')
            if not definition or not definition.get_path_name().startswith('/Game/Aurelion/') or not definition.get_editor_property('allow_multiple_instances'):
                raise RuntimeError('Placed identity requires its owned repeatable definition: '+label)
            if actor.get_editor_property('npc_definition') is not None:
                raise RuntimeError('Identity authoring must precede native definition initialization: '+label)
            info = _copy(actor.get_editor_property('spawn_info'))
            if (_digits(info.get_editor_property('owning_spawner_guid')) != ZERO
                    or str(info.get_editor_property('spawn_name')) != 'None'
                    or info.get_editor_property('owning_spawn') is not None):
                raise RuntimeError('A spawner owns this actor; its metadata will not be rewritten: '+label)
            key, guid = identity_guid(map_package, label)
            if guid in guids:
                raise RuntimeError('Duplicate deterministic GUID in placed NPC census')
            guids.add(guid)
            before = _digits(info.get_editor_property('spawn_assigned_save_guid'))
            if before not in (ZERO, guid):
                raise RuntimeError('An existing different save identity will not be overwritten: '+label)
            value = unreal.Guid()
            value.import_text(guid)  # Installed FGuid::ImportTextItem accepts exact 32-digit text.
            if _digits(value) != guid:
                raise RuntimeError('FGuid conversion changed the deterministic identity')
            row = dict(map=map_package, identity_kind=kind, semantic_identity=semantic,
                actor_label=label, actor_path=actor.get_path_name(), identity_key=key,
                guid=guid, previous_guid=before, interface_guid=None,
                supplied_identity_preserved=before in (ZERO, guid),
                other_spawn_metadata_unchanged=False, definition_uninitialized=False)
            evidence['rows'].append(row)
            planned.append((actor, info, value, row, _other_metadata(info)))
        for actor, info, value, row, other_before in planned:
            _assert_world(context)
            assigned = unreal.SovAurelionNPCIdentityLibrary.assign_owned_placed_npc_identity(actor, value)
            if not assigned.get_editor_property('succeeded'):
                raise RuntimeError('Native placed identity assignment refused '+row['actor_label']+': '
                                   +str(assigned.get_editor_property('report')))
            actual = actor.get_editor_property('spawn_info')
            if _digits(actual.get_editor_property('spawn_assigned_save_guid')) != row['guid']:
                raise RuntimeError('Placed SpawnInfo GUID readback failed: '+row['actor_label'])
            row['other_spawn_metadata_unchanged'] = _other_metadata(actual) == other_before
            row['interface_guid'] = _digits(actor.call_method('GetActorGUID'))
            row['definition_uninitialized'] = actor.get_editor_property('npc_definition') is None
            if not (row['other_spawn_metadata_unchanged'] and row['definition_uninitialized']
                    and row['interface_guid'] == row['guid']):
                raise RuntimeError('Actual pre-BeginPlay Narrative identity contract failed: '+row['actor_label'])
            evidence['checked'] += 1
        _assert_world(context)
        if any(a.get_editor_property('spawn_info').export_text() != before for a, before in untouched):
            raise RuntimeError('Nonowned NPC spawn metadata changed during identity authoring')
        mapping = {row['identity_key']: row['guid'] for row in evidence['rows']}
        evidence['identity_map_sha256'] = hashlib.sha256(json.dumps(mapping, sort_keys=True, separators=(',', ':')).encode('utf-8')).hexdigest()
        evidence['nonowned_metadata_unchanged'] = True
        evidence['nonowned_npcs_checked'] = len(untouched)
        evidence['status'] = 'passed'
        return evidence
    except Exception as error:
        evidence['status'] = 'failed'
        evidence['failures'].append(str(error))
        raise
