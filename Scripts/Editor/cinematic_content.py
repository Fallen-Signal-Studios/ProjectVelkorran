"""Author Aurelion's eighteen visible, readable native-owned story scenes.

Entry: build_cinematics({chapter, world, api, heroes, existing, positions}).
api is setup_aurelion_full_route, not a second asset owner. Call once in each
fresh owned wrapper; retain UObject results only while that wrapper is loaded.
No journal mutation, receipt injection, editor launch, build or package save of
the current world occurs here. Root saves/qualifies the assembled world.

Epic API references verified locally in UE5.7:
SequencerScripting/Content/Python/sequencer_examples.py (spawnable cameras),
MovieSceneSequenceExtensions.h (get_binding_id, add_track, playback bounds),
MovieSceneBindingExtensions.h (object templates and binding tracks),
MovieSceneCameraCutSection.h (set_camera_binding_id), and KismetSystemLibrary.h.
"""
from __future__ import annotations

import math
import unreal
from story_content import SCENES

FPS = 24
MAPS = {12: '/Game/Aurelion/Maps/L_Aurelion_M12',
        13: '/Game/Aurelion/Maps/L_Aurelion_M13'}
IDS = {12: 'M12_FireAndFrost', 13: 'M13_ContraryWitness'}
SOLO_SCENES = {'TarrikIndependentAssent', 'SeleneIndependentAssent'}
M12_BEATS = {row[0] for row in SCENES[:8]}

# These are graybox representations of actual named/preserved people, not enemies
# or alternate protagonist proxies. Existing campaign owners decide their outcomes.
CAST = {
    'MeetingTarrik': dict(name='Tarrik', look='Tarrik', at=(1250, -950, 90)),
    'TrappedMarine': dict(name='Trapped marine', look='Tarrik', at=(660, 8810, -410)),
    'Lyric': dict(name='Lyric', look='Selene', at=(1150, 9760, -410)),
    'Malik': dict(name='Malik', look='Tarrik', at=(1150, 14300, -810)),
    'Tharne': dict(name='Tharne', look='Tarrik', at=(-1150, 14300, -810)),
    'Lyessa': dict(name='Lyessa', look='Selene', at=(-1000, 14700, -810)),
    'WestDominionStretcher': dict(name='Dominion stretcher patient', look='Tarrik', at=(-1250, 15100, -810)),
    'WestReformationStretcher': dict(name='Reformation stretcher patient', look='Selene', at=(-1000, 15300, -810)),
    'EastDominionWalker': dict(name='Dominion walking wounded', look='Tarrik', at=(1000, 15100, -810)),
    'EastReformationWalker': dict(name='Reformation walking wounded', look='Selene', at=(1250, 15300, -810)),
}
# Manuscript chapter24: Malik and Lyessa are Dominion; Tharne and Lyric are
# Reformation; the marine Tarrik frees is explicitly Reformation (printed p464).
# Temporary body/appearance choices never determine canonical faction membership.
CAST_FACTIONS = {
    'MeetingTarrik': 'Tarrik', 'TrappedMarine': 'Selene', 'Lyric': 'Selene',
    'Malik': 'Tarrik', 'Tharne': 'Selene', 'Lyessa': 'Tarrik',
    'WestDominionStretcher': 'Tarrik', 'WestReformationStretcher': 'Selene',
    'EastDominionWalker': 'Tarrik', 'EastReformationWalker': 'Selene',
}
EXTRA_CAST = {
    'FreeTrappedMarine': ['TrappedMarine'],
    'GroundLyric': ['Lyric'],
    'DestroyDominionResonator': ['Tharne'],
    'DestroyReformationCage': ['Malik'],
    'ShareIsolatedThreatData': ['Malik', 'Tharne', 'Lyessa', 'WestDominionStretcher',
                              'WestReformationStretcher', 'EastDominionWalker', 'EastReformationWalker'],
    'SurvivorsClearAndQuarantine': [k for k in CAST if k != 'MeetingTarrik'],
}

# Both groups physically wait in their protected Z08 recesses before the player
# chooses which group leaves first. These are native cinematic exit postconditions.
E4_REFUGE_MARKS = {
    'Tharne': (-2750., 22150., -1110.),
    'Lyessa': (-2350., 22150., -1110.),
    'WestDominionStretcher': (-2850., 22400., -1110.),
    'WestReformationStretcher': (-2350., 22680., -1110.),
    'Malik': (3050., 22150., -1110.),
    'EastDominionWalker': (2890., 22580., -1110.),
    'EastReformationWalker': (3200., 22680., -1110.),
}


def _xyz(v):
    return (float(v.x), float(v.y), float(v.z))


def _add(a, b):
    return tuple(float(x) + float(y) for x, y in zip(a, b))


def _rotation(yaw=90., pitch=0., roll=0.):
    return unreal.Rotator(pitch=float(pitch), yaw=float(yaw), roll=float(roll))


def _look_at(position, target):
    d = tuple(b-a for a, b in zip(position, target))
    return _rotation(math.degrees(math.atan2(d[1], d[0])),
                     math.degrees(math.atan2(d[2], math.hypot(d[0], d[1]))))


def _assert_world(ctx):
    chapter = ctx['chapter']
    current = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if chapter not in MAPS or editor.is_in_play_in_editor() or current != ctx['world']:
        raise RuntimeError('Cinematic authoring requires its stopped, exact current wrapper world.')
    if current.get_path_name().split('.')[0] != MAPS[chapter]:
        raise RuntimeError('Refusing cinematic changes outside the exact Aurelion mission wrapper.')
    return current


def _result(result, operation):
    # Proven UE Python UPROPERTY bool convention: bSucceeded becomes succeeded.
    if not result.get_editor_property('succeeded'):
        raise RuntimeError(operation + ': ' + result.get_editor_property('report'))
    return result


def _spawn(ctx, cls, name, position, yaw=90., folder='Aurelion/Story'):
    _assert_world(ctx)
    actor = ctx['api'].spawn(cls, name, position, yaw=yaw, folder=folder)
    if not actor.get_path_name().startswith(ctx['world'].get_path_name()+':'):
        raise RuntimeError('Spawn returned an actor outside the current owned world.')
    ctx['_new_actors'].append(actor)
    return actor


def _trace_hit(result):
    if isinstance(result, unreal.HitResult):
        return result
    if isinstance(result, tuple):
        hits = [value for value in result if isinstance(value, unreal.HitResult)]
        if len(hits) == 1 and not any(value is False for value in result):
            return hits[0]
    return None


def _safe_character_point(ctx, position, name, character):
    """Validate actual collision, not the bounding box of an atrium ring/hole."""
    _assert_world(ctx)
    if len(position) != 3 or not all(math.isfinite(x) for x in position):
        raise RuntimeError('Non-finite character mark: '+name)
    radius, half_height = _capsule_dimensions(name, character)
    # The root stations use approximate pawn-center heights; trace the real floor.
    feet = position[2] - half_height
    result = unreal.SystemLibrary.line_trace_single(
        ctx['world'], unreal.Vector(position[0], position[1], position[2]+half_height+80.),
        unreal.Vector(position[0], position[1], feet-600.),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, ctx['_new_actors'],
        unreal.DrawDebugTrace.NONE, True)
    hit = _trace_hit(result)
    # FHitResult's native members are not reflected UPROPERTY fields. Use the
    # native breaker exposed by HitResult.to_tuple(): blocking, overlap, time, distance,
    # location, impact point, normal, impact normal, then hit identities.
    values = hit.to_tuple() if hit is not None else None
    if not isinstance(values, tuple) or len(values) < 8 or not values[0]:
        raise RuntimeError('No physical floor below authored character mark '+name+': '+str(position))
    if values[1]:
        raise RuntimeError('Floor trace starts inside geometry at '+name+': '+str(position))
    normal = values[7]
    point = values[5]
    if normal.z < .7 or not all(math.isfinite(v) for v in _xyz(point)):
        raise RuntimeError('Character mark is not on a walkable floor: '+name)
    settled = (float(position[0]), float(position[1]), float(point.z)+half_height+2.)
    ctx['_report']['floor_marks'][name] = {'requested': list(position), 'floor': list(_xyz(point)), 'center': list(settled),
                                          'character': character.get_path_name(), 'radius': radius, 'half_height': half_height}
    return settled


def _transform(position, yaw=90., scale=(1., 1., 1.)):
    return unreal.Transform(location=unreal.Vector(*position),
                            rotation=_rotation(yaw), scale=unreal.Vector(*scale))


def _clear_binding_presentation(binding):
    # Keep spawn sections, which own camera/prop lifetime. All other tracks in these
    # exact stamped, generated sequences are rebuilt deterministically by this module.
    for track in list(binding.get_tracks()):
        if not isinstance(track, unreal.MovieSceneSpawnTrack):
            binding.remove_track(track)


def _transform_track(binding, frames, start, end=None, start_rotation=None, end_rotation=None, scale=(1., 1., 1.)):
    start_rotation = start_rotation or _rotation()
    end_rotation = end_rotation or start_rotation
    end = end or start
    section = binding.add_track(unreal.MovieScene3DTransformTrack).add_section()
    section.set_range(0, frames)
    section.set_completion_mode(unreal.MovieSceneCompletionMode.RESTORE_STATE)
    channels = section.get_channels_by_type(unreal.MovieSceneScriptingDoubleChannel)
    if len(channels) != 9:
        raise RuntimeError('UE transform track did not expose the expected nine double channels.')
    start_values = tuple(start)+(start_rotation.roll, start_rotation.pitch, start_rotation.yaw)+tuple(scale)
    end_values = tuple(end)+(end_rotation.roll, end_rotation.pitch, end_rotation.yaw)+tuple(scale)
    for channel, first, last in zip(channels, start_values, end_values):
        channel.set_default(float(first))
        channel.add_key(unreal.FrameNumber(0), float(first), 0., unreal.MovieSceneTimeUnit.DISPLAY_RATE,
                        unreal.MovieSceneKeyInterpolation.LINEAR)
        channel.add_key(unreal.FrameNumber(frames-1), float(last), 0., unreal.MovieSceneTimeUnit.DISPLAY_RATE,
                        unreal.MovieSceneKeyInterpolation.LINEAR)
    return section


def _existing_binding(sequence, name):
    matches = [b for b in sequence.get_bindings() if b.get_name() == name]
    if len(matches) > 1:
        raise RuntimeError('Ambiguous authored sequence binding: '+name)
    return matches[0] if matches else None


def _character_binding(sequence, tag):
    result = _result(unreal.SovAurelionAuthoringLibrary.add_aurelion_character_binding(sequence, unreal.Name(tag)), 'Character binding '+tag)
    binding = sequence.find_binding_by_id(result.get_editor_property('binding'))
    if not binding.is_valid():
        raise RuntimeError('Native character binding did not resolve in its sequence.')
    _clear_binding_presentation(binding)
    return binding


def _camera(ctx, sequence, name, frames, position, target, focal=28., drift=(30., 0., 0.)):
    binding = _existing_binding(sequence, name)
    if binding is not None:
        binding.remove()
    # UE5.7 CreateSpawnable(Class) produced an incomplete camera archetype in the
    # actual editor. A real owned camera instance has initialized native components
    # and stays in the exact same persistent wrapper as its sequence actor.
    camera = _spawn(ctx, unreal.CineCameraActor, sequence.get_name()+'_'+name, position, 0.)
    camera.set_actor_rotation(_look_at(position, target), False)
    component = camera.get_cine_camera_component() or camera.get_component_by_class(unreal.CineCameraComponent)
    if not isinstance(component, unreal.CineCameraComponent):
        raise RuntimeError('The actual owned CineCameraActor has no initialized CineCameraComponent.')
    component.set_editor_property('current_focal_length', float(focal))
    focus = component.get_editor_property('focus_settings')
    focus.set_editor_property('focus_method', unreal.CameraFocusMethod.DISABLE)
    component.set_editor_property('focus_settings', focus)
    binding = sequence.add_possessable(camera)
    binding.set_name(name)
    if not binding.is_valid():
        raise RuntimeError('Could not bind the actual owned cinematic camera.')
    _clear_binding_presentation(binding)
    ending = _add(position, drift)
    _transform_track(binding, frames, position, ending, _look_at(position, target), _look_at(ending, target))
    return binding


def _cue_plan(beat, lines):
    cursor = 1.0
    result = []
    for index, (speaker, words) in enumerate(lines):
        duration = math.ceil(max(4., len(words)/20.+1.25)*FPS)/FPS
        result.append(dict(key=f'{beat}.{index:02d}', start=cursor, duration=duration, speaker=speaker, text=words, priority='UNSET'))
        cursor += duration + .35
    if beat == 'VoluntaryStay':
        branches = [
            ('WEST_STRETCHERS', 'Tarrik', 'We made room for those who could not run. The stretchers went first, and both groups made it.'),
            ('EAST_WALKERS', 'Selene', 'We trusted the others with the route. The walkers went first, and helped bring the last stretcher through.'),
        ]
        duration = math.ceil(max(len(words)/20.+1.25 for _, _, words in branches)*FPS)/FPS
        for priority, speaker, words in branches:
            result.append(dict(key=beat+'.'+priority, start=cursor, duration=duration, speaker=speaker, text=words, priority=priority))
        cursor += duration + .35
    return result, math.ceil((cursor+1.0)*FPS)


def _dialogue(ctx, plan):
    result = []
    for row in plan:
        cue = unreal.SovAurelionDialogueCue()
        ctx['api'].prop(cue, start_seconds=float(row['start']), duration_seconds=float(row['duration']),
                        speaker=ctx['api'].text('Speaker.'+row['speaker'], row['speaker']),
                        text=ctx['api'].text('Scene.'+row['key'], row['text']),
                        required_priority=getattr(unreal.SovAurelionRescuePriority, row['priority']))
        result.append(cue)
    return result


def _participant(binding, actor_tag=None, controlled=False, exit_at=None, exit_yaw=90.):
    value = unreal.SovCinematicParticipant()
    value.set_editor_property('binding_tag', unreal.Name(binding))
    value.set_editor_property('controlled_protagonist', bool(controlled))
    value.set_editor_property('actor_tag', unreal.Name(actor_tag or ''))
    value.set_editor_property('require_living', True)
    value.set_editor_property('exit_wield', unreal.SovCinematicExitWield.HOLSTER)
    if exit_at is not None:
        value.set_editor_property('apply_exit_transform', True)
        value.set_editor_property('exit_transform', _transform(exit_at, exit_yaw))
    return value


def _capsule_dimensions(name, character):
    capsule = character.get_editor_property('capsule_component')
    radius = float(capsule.get_scaled_capsule_radius())
    half_height = float(capsule.get_scaled_capsule_half_height())
    if not all(math.isfinite(v) and v > 0. for v in (radius, half_height)) or half_height < radius:
        raise RuntimeError('Invalid actual participant capsule: '+name)
    return radius, half_height


def _exit_capsule(name, character, position):
    radius, half_height = _capsule_dimensions(name, character)
    return dict(name=name, center=list(position), radius=radius, half_height=half_height)


def _validate_exit_pairs(beat, capsules):
    # Match native SovCinematicPolicy::CapsulesOverlap, using the actual authored
    # pawn/companion class defaults and placed cast dimensions rather than a guess.
    for i, a in enumerate(capsules):
        for b in capsules[i+1:]:
            xy_squared = sum((a['center'][axis]-b['center'][axis])**2 for axis in (0, 1))
            if (xy_squared < (a['radius']+b['radius'])**2
                    and abs(a['center'][2]-b['center'][2]) < a['half_height']+b['half_height']):
                raise RuntimeError('Overlapping cinematic exit capsules: '+beat+' '+a['name']+' / '+b['name'])


def _static(ctx, name, position, scale, material, yaw=0., pitch=0., roll=0.):
    actor = _spawn(ctx, unreal.StaticMeshActor, name, position, yaw)
    actor.static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    actor.static_mesh_component.set_static_mesh(ctx['api'].required('/Engine/BasicShapes/Cube'))
    actor.static_mesh_component.set_material(0, ctx['api'].required('/Game/Aurelion/Materials/'+material))
    actor.set_actor_scale3d(unreal.Vector(*scale))
    actor.set_actor_rotation(_rotation(yaw, pitch, roll), False)
    return actor


def _carrier(ctx):
    """Visible rescued craft beside the bridge; success presentation is journal-derived."""
    # Keep the entire craft east of the playable connector, including its west engine.
    hull_position = (11000., 500., -1000.)
    stable_position = (11000., 500., 250.)
    groups = []
    scenery = []
    for stable in (False, True):
        suffix = 'Stable' if stable else 'Suspended'
        center = stable_position if stable else hull_position
        hull = _static(ctx, 'Carrier_'+suffix, center, (44., 105., 12.), 'M_GB_scenic', yaw=0., roll=0. if stable else 13.)
        parts = [hull]
        for side in (-1, 1):
            wing = _static(ctx, f'Carrier_{suffix}_Engine{side}', _add(center, (side*2600., 500., 250.)), (12., 60., 8.), 'M_GB_blue')
            wing.attach_to_actor(hull, unreal.Name(''), unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, False)
            parts.append(wing)
        nose = _static(ctx, 'Carrier_'+suffix+'_Forward', _add(center, (0., 5300., 250.)), (35., 20., 10.), 'M_GB_amber', pitch=-15.)
        nose.attach_to_actor(hull, unreal.Name(''), unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, False)
        parts.append(nose)
        for part in parts:
            component = part.static_mesh_component
            # Journal gates still own actor visibility/collision flags. Their later
            # re-enable cannot turn this deliberately noncolliding scenery into a wall.
            component.set_collision_profile_name('NoCollision')
            component.set_editor_property('can_ever_affect_navigation', False)
            if (str(component.get_collision_profile_name()) != 'NoCollision'
                    or component.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION
                    or component.get_editor_property('can_ever_affect_navigation')):
                raise RuntimeError('Carrier scenery retained collision/navigation: '+part.get_path_name())
            origin, extent, _ = unreal.SystemLibrary.get_component_bounds(component)
            if origin.x-extent.x < 7800.-.1:
                raise RuntimeError('Carrier scenery still intrudes toward the connector: '+part.get_path_name())
            scenery.append({'actor': part.get_path_name(), 'component': component.get_path_name(),
                'center': _xyz(origin), 'extent': _xyz(extent), 'min_x': origin.x-extent.x,
                'collision_profile': str(component.get_collision_profile_name()), 'can_ever_affect_navigation': False})
        groups.append((hull, parts))
    gates = [ctx['api'].gate(12, 'MeetingAndCarrierRescue', 'CarrierApproachPresentation', (0, 0, 0), targets=groups[0][1]),
             ctx['api'].gate(12, 'MeetingAndCarrierRescue', 'CarrierRescuedPresentation', (0, 0, 0), close_after=True, targets=groups[1][1])]
    ctx['_report']['carrier'] = {'type': 'compound graybox craft', 'animated_hull': groups[0][0].get_path_name(),
                                'stable_hull': groups[1][0].get_path_name(), 'start': hull_position, 'end': stable_position,
                                'canonical_owner': 'MeetingAndCarrierRescue native cinematic receipt',
                                'scenery_components': scenery, 'connector_outer_x': 7300.,
                                'rendered_framing_verified': False}
    return groups[0][0], hull_position, stable_position, gates


def _cast(ctx):
    if ctx['chapter'] != 12:
        return {}, {}, []
    api = ctx['api']
    story_bp = api.bp('/Game/Aurelion/Characters/BP_AurelionStoryNPC', unreal.SovNPCCharacterBase)
    controller = unreal.load_class(None, '/NarrativePro/Pro/Core/AI/BP/BP_NarrativeNPCController.BP_NarrativeNPCController_C')
    if controller is None:
        raise RuntimeError('Stock Narrative NPC controller is unavailable.')
    cdo = unreal.get_default_object(story_bp.generated_class())
    api.prop(cdo, ai_controller_class=controller, auto_possess_ai=unreal.AutoPossessAI.PLACED_IN_WORLD_OR_SPAWNED)
    api.compile_bp(story_bp)
    cdo = unreal.get_default_object(story_bp.generated_class())
    pacifist = api.required('/NarrativePro/Pro/Core/AI/Configs/AC_Pacifist')
    attributes = api.required('/NarrativePro/Pro/Core/Abilities/Configurations/AC_NPC_Default')
    actors, definitions, gates = {}, {}, []
    if set(CAST_FACTIONS) != set(CAST):
        raise RuntimeError('Every authored cast identity requires an explicit canonical faction.')
    for identity, data in CAST.items():
        definition = api.new_asset('/Game/Aurelion/Characters/NPC_Aurelion'+identity, unreal.NPCDefinition)
        source = ctx['heroes'][data['look']]['player']
        faction_source = ctx['heroes'][CAST_FACTIONS[identity]]['player']
        api.prop(definition, character_id=unreal.Name('Aurelion_'+identity), npc_name=api.text('Cast.'+identity, data['name']),
                 npc_class_path=story_bp.generated_class(), default_appearance=source.get_editor_property('default_appearance'),
                 default_factions=faction_source.get_editor_property('default_factions'), activity_configuration=pacifist,
                 ability_configuration=attributes, allow_multiple_instances=True, default_currency=0, trading_currency=0,
                 is_vendor=False, default_item_loadout=[], trading_item_loadout=[])
        api.retain_aurelion_npc(definition)
        api.save(definition)
        position = _safe_character_point(ctx, data['at'], 'Cast.'+identity, cdo)
        actor = _spawn(ctx, story_bp.generated_class(), 'Cast_'+identity, position)
        if _capsule_dimensions(identity, actor) != _capsule_dimensions(identity, cdo):
            raise RuntimeError('Spawn changed the authored story capsule dimensions: '+identity)
        api.prop(actor, authored_placed_definition=definition,
                 tags=list(actor.tags)+[unreal.Name('Aurelion_'+identity)])
        actors[identity] = actor
        definitions[identity] = definition
        ctx['_report']['cast'][identity] = {'actor': actor.get_path_name(), 'definition': definition.get_path_name(),
                                          'initial_position': list(position), 'appearance_placeholder': data['look'],
                                          'faction_source': faction_source.get_path_name(), 'invulnerability_added': False}
    # The unregistered meeting stand-in is presentation only, and stops appearing once
    # the genuine shared handoff creates the controller-owned protagonist companion.
    gates.append(api.gate(12, 'HandoffToTarrikRescue', 'MeetingTarrikRetirement', (0, 0, 0), targets=[actors['MeetingTarrik']]))
    return actors, definitions, gates


def _cage_presentation(ctx, beat):
    if beat not in ('DestroyDominionResonator', 'DestroyReformationCage'):
        return []
    faction = 'DOMINION' if beat == 'DestroyDominionResonator' else 'REFORMATION'
    # The imported layout calls both structures Cage. Match the named faction,
    # never hide both structures at the second beat or add a duplicate first cage.
    matches = [actor for label, actor in ctx['existing'].items()
               if label.startswith('Z07_Cage_') and ('_'+faction) in label]
    if not matches:
        raise RuntimeError('The approved imported layout is missing its exact '+faction+' cage geometry.')
    return [ctx['api'].gate(12, beat, beat+'_PhysicalFrame', (0, 0, 0), targets=matches)]


def _scene(ctx, row, cast, logical_positions, carrier):
    api = ctx['api']
    beat, zone, hero, lines = row
    hero_character = unreal.get_default_object(ctx['heroes'][hero]['pawn'])
    partner = 'Selene' if hero == 'Tarrik' else 'Tarrik'
    partner_character = None
    if beat not in SOLO_SCENES:
        partner_character = (cast['MeetingTarrik'] if beat == 'MeetingAndCarrierRescue'
                             else unreal.get_default_object(ctx['heroes'][partner]['companion_bp'].generated_class()))
    station = _safe_character_point(ctx, ctx['positions'][beat], beat+'.Hero', hero_character)
    hero_exit = station
    # The quarantine exit ramp is only six metres wide (rails at x=+/-288).
    # Keep the partner beside Hero, before the three survivor rows, not off its edge.
    partner_offset = {
        'SurvivorsClearAndQuarantine': (150., 0., 0.),
        'DestroyDominionResonator': (150., 0., 0.),
        'DestroyReformationCage': (150., 0., 0.),
        'MeridianContainment': (0., 180., 0.),
        'FifthWitness': (-150., 180., 0.),
        'VoluntaryStay': (150., 0., 0.),
    }.get(beat, (150., 180., 0.))
    # Every mark traces from the original authored height. Reusing Hero's settled
    # height can start a nearby uphill trace below its own surface.
    partner_start = _safe_character_point(ctx, _add(ctx['positions'][beat], partner_offset), beat+'.Partner', partner_character) if partner_character is not None else None
    partner_exit = partner_start
    if beat == 'SeparateDepartures':
        hero_exit = _safe_character_point(ctx, (-1350., 48000., 90.), beat+'.TarrikExit', hero_character)
        partner_exit = _safe_character_point(ctx, (1350., 48000., 90.), beat+'.SeleneExit', partner_character)
    plan, frames = _cue_plan(beat, lines)
    sequence = api.new_asset('/Game/Aurelion/Cinematics/LS_'+beat, unreal.LevelSequence, unreal.LevelSequenceFactoryNew())
    for track in list(sequence.get_tracks()):
        if isinstance(track, unreal.MovieSceneCameraCutTrack):
            sequence.remove_track(track)
        else:
            raise RuntimeError('Unexpected top-level track in owned story sequence: '+track.get_class().get_name())
    sequence.set_display_rate(unreal.FrameRate(FPS, 1))
    sequence.set_playback_start(0)
    sequence.set_playback_end(frames)
    participants = [_participant('Hero', controlled=True, exit_at=hero_exit)]
    exit_capsules = [_exit_capsule('Hero', hero_character, hero_exit)]
    binding = _character_binding(sequence, 'Hero')
    _transform_track(binding, frames, station, hero_exit)
    if partner_start:
        actor_tag = 'Aurelion_'+partner+'Actor'
        if beat == 'MeetingAndCarrierRescue':
            actor_tag = 'Aurelion_MeetingTarrik'
        exit_capsules.append(_exit_capsule('Partner', partner_character, partner_exit))
        participants.append(_participant('Partner', actor_tag, exit_at=partner_exit))
        binding = _character_binding(sequence, 'Partner')
        _transform_track(binding, frames, partner_start, partner_exit, _rotation(-90.), _rotation(-90.))
    for identity in EXTRA_CAST.get(beat, []):
        start = logical_positions[identity]
        ending = start
        if beat == 'FreeTrappedMarine':
            ending = _safe_character_point(ctx, (650., 9170., -410.), beat+'.MarineClear', cast[identity])
        elif beat == 'GroundLyric':
            ending = _safe_character_point(ctx, (-900., 14500., -810.), beat+'.LyricRefuge', cast[identity])
        elif beat == 'ShareIsolatedThreatData':
            ending = _safe_character_point(ctx, E4_REFUGE_MARKS[identity], beat+'.Refuge.'+identity, cast[identity])
        elif beat == 'SurvivorsClearAndQuarantine':
            # Actual native transforms put every surviving protected person beyond
            # the sealed arena, rather than turning a spoken line into physical proof.
            index = EXTRA_CAST[beat].index(identity)
            ending = _safe_character_point(ctx, (-120.+120.*(index%3), 23700.+150.*(index//3), -1110.), beat+'.Safe.'+identity, cast[identity])
        participants.append(_participant(identity, 'Aurelion_'+identity, exit_at=ending))
        exit_capsules.append(_exit_capsule(identity, cast[identity], ending))
        binding = _character_binding(sequence, identity)
        # Long relocations belong only to the native, validated final cut. Leave
        # their tagged participant at its actual pose during playback: even a
        # constant old start mark can be above a cage surface already removed.
        distance = math.dist(start, ending)
        if distance <= 1500.:
            _transform_track(binding, frames, start, ending)
        ctx['_report'].setdefault('cast_exits', {}).setdefault(beat, {})[identity] = {
            'start': list(start), 'end': list(ending),
            'presentation': 'visible short move' if distance <= 1500. else 'native final-cut relocation'}
        logical_positions[identity] = ending
    _validate_exit_pairs(beat, exit_capsules)
    # Native exit collision ignores this scene's participants, not all other NPCs.
    # Track the already committed cast exits so an unrelated survivor cannot be
    # silently assumed absent simply because editor authoring left it at spawn.
    nonparticipants = []
    for identity, character in cast.items():
        if identity in EXTRA_CAST.get(beat, []) or identity == 'MeetingTarrik':
            # The stand-in is this scene's Partner at Meeting and is retired by
            # the genuine first shared handoff before every later scene.
            continue
        nonparticipants.append(_exit_capsule(identity, character, logical_positions[identity]))
    _validate_exit_pairs(beat+'.LogicalCast', exit_capsules+nonparticipants)
    target = _add(station, (60., 80., 60.))
    wide_position = _add(station, (460., -520., 220.))
    close_position = _add(station, (-340., -330., 145.))
    wide_target, wide_focal = target, 26.
    if beat == 'MeetingAndCarrierRescue':
        hull, start, ending = carrier
        binding = _existing_binding(sequence, 'Carrier')
        if binding is not None:
            # Root replaces stamped actors on rerun; never retain the previous
            # map-instance reference in this same-world possessable binding.
            binding.remove()
        binding = sequence.add_possessable(hull)
        binding.set_name('Carrier')
        _clear_binding_presentation(binding)
        _transform_track(binding, frames, start, ending, _rotation(0., 0., 13.), _rotation(0.), _xyz(hull.get_actor_scale3d()))
        # Back the wide shot away from both subjects so the eastward craft and
        # the unchanged meeting station remain inside the same establishing frame.
        wide_position, wide_target, wide_focal = (2500., -14000., 5000.), (7000., 500., 0.), 18.
    wide = _camera(ctx, sequence, 'CameraWide', frames, wide_position, wide_target, wide_focal)
    close = _camera(ctx, sequence, 'CameraClose', frames, close_position, target, 32.)
    cuts = sequence.add_track(unreal.MovieSceneCameraCutTrack)
    cut_frame = max(FPS, min(frames-FPS, round(plan[max(1, len(plan)//2)]['start']*FPS)))
    for camera, first, last in ((wide, 0, cut_frame), (close, cut_frame, frames)):
        cut = cuts.add_section()
        cut.set_range(first, last)
        cut.set_camera_binding_id(sequence.get_binding_id(camera))
    actor = _spawn(ctx, unreal.SovAurelionStorySequenceActor, 'Scene_'+beat, station)
    actor.set_sequence(sequence)
    api.prop(actor.campaign_cinematic, mission_id=unreal.Name(IDS[ctx['chapter']]), beat_id=unreal.Name(beat),
             sequence=sequence, participants=participants, request_range=400.)
    api.prop(actor, dialogue_cues=_dialogue(ctx, plan))
    ctx['_report']['scenes'][beat] = {'sequence': sequence.get_path_name(), 'actor': actor.get_path_name(),
                                     'hero': hero, 'zone': zone, 'seconds': frames/FPS,
                                     'camera_count': 2, 'camera_cut_frames': [0, cut_frame, frames],
                                     'wide_camera': {'position': list(wide_position), 'target': list(wide_target),
                                                     'focal_length': wide_focal, 'rendered_framing_verified': False},
                                     'participant_tags': [str(p.binding_tag) for p in participants],
                                     'hero_exit': list(hero_exit), 'partner_exit': list(partner_exit) if partner_exit else None,
                                     'exit_capsules': exit_capsules, 'exit_pairs_valid': True,
                                     'nonparticipant_capsules': nonparticipants, 'logical_cast_pairs_valid': True,
                                     'cues': plan, 'playback_verified': False}
    return sequence, actor


def validate_static_scene_exits(context, scene_result):
    """Read-only callback AFTER author_route/encounter/gate/request construction.

    Calls the native editor helper's ECC_Pawn query with the actual dimensions
    already used for each exit. It does not move actors, toggle gates/collision,
    save packages, or claim runtime navigation or scene completion.
    The mutable report is installed before checks so a failing batch retains all
    checked marks and exact blocker paths in root's existing failure report.
    """
    ctx = dict(context)
    _assert_world(ctx)
    report = {'checked': 0, 'clear': False, 'runtime_admission_verified': False,
              'collision_channel': 'ECC_Pawn', 'ignored': 'Narrative characters and their attachments; logical cast checked separately',
              'marks': [], 'failures': []}
    scene_result['report']['static_exit_geometry'] = report
    for beat, scene in scene_result['report']['scenes'].items():
        if not scene.get('exit_pairs_valid') or not scene.get('logical_cast_pairs_valid'):
            raise RuntimeError('Missing logical actor-pair preflight for '+beat)
        actor = scene_result['scenes'][beat]
        participants = {str(p.binding_tag): p for p in actor.campaign_cinematic.participants}
        for mark in scene['exit_capsules']:
            _assert_world(ctx)
            participant = participants.get(mark['name'])
            if participant is None or not participant.apply_exit_transform:
                raise RuntimeError('Scene report and native participant exits differ: '+beat+'.'+mark['name'])
            transform = participant.exit_transform
            native_center = unreal.MathLibrary.transform_location(transform, unreal.Vector(0., 0., 0.))
            if math.dist(_xyz(native_center), mark['center']) > .1:
                raise RuntimeError('Scene report and native exit transform differ: '+beat+'.'+mark['name'])
            result = unreal.SovAurelionSceneValidationLibrary.validate_aurelion_exit_geometry(
                ctx['world'], transform, float(mark['radius']), float(mark['half_height']))
            item = {'beat': beat, 'participant': mark['name'], 'center': list(mark['center']),
                    'radius': mark['radius'], 'half_height': mark['half_height'],
                    'query_succeeded': bool(result.succeeded), 'clear': bool(result.clear),
                    'recovery_excluded': bool(result.recovery_excluded), 'report': str(result.report),
                    'blocking_components': list(result.blocking_components)}
            report['marks'].append(item); report['checked'] += 1
            if not result.succeeded or not result.clear:
                report['failures'].append(beat+'.'+mark['name'])
    report['clear'] = not report['failures']
    if not report['clear']:
        raise RuntimeError('Static cinematic exit geometry failed: '+', '.join(report['failures']))
    return report


def build_cinematics(context):
    """Author one exact loaded wrapper. See module docstring for its explicit API.

    Result has sequences/scenes/cast/cast_definitions/gates/report. Root creates
    mission assets, requests, encounters and checkpoint bindings, then saves the map.
    """
    ctx = dict(context)
    _assert_world(ctx)
    ctx['_new_actors'] = []
    ctx['_report'] = {'chapter': ctx['chapter'], 'map': MAPS[ctx['chapter']], 'scenes': {}, 'cast': {}, 'floor_marks': {}}
    api = ctx['api']
    if not all(key in ctx for key in ('heroes', 'existing', 'positions')):
        raise RuntimeError('Missing explicit cinematic context: heroes, existing, positions.')
    cast, definitions, gates = _cast(ctx)
    logical = {identity: _xyz(actor.get_actor_location()) for identity, actor in cast.items()}
    carrier = None
    if ctx['chapter'] == 12:
        hull, start, ending, carrier_gates = _carrier(ctx)
        carrier = hull, start, ending
        gates.extend(carrier_gates)
    sequences, scenes = {}, {}
    selected = [row for row in SCENES if (row[0] in M12_BEATS) == (ctx['chapter'] == 12)]
    if len(selected) != (8 if ctx['chapter'] == 12 else 10):
        raise RuntimeError('The approved eighteen-scene manifest is incomplete.')
    for row in selected:
        _assert_world(ctx)
        sequence, actor = _scene(ctx, row, cast, logical, carrier)
        sequences[row[0]], scenes[row[0]] = sequence, actor
        gates.extend(_cage_presentation(ctx, row[0]))
    # Materialize table text before native readable-duration validation. The helper
    # validates the entire batch and can write only this exact owned string table.
    table = api.required('/Game/Aurelion/Data/ST_AurelionText')
    _result(unreal.SovAurelionAuthoringLibrary.set_aurelion_strings(table, api.strings), 'Aurelion scene strings')
    api.save(table)
    for beat, actor in scenes.items():
        _assert_world(ctx)
        result = actor.validate_story_content()
        # UE's bool-plus-single-out wrapper exposes None for failure and the output
        # string on success. Accept a native empty success string; reject a reason.
        if result is None or (isinstance(result, str) and result):
            raise RuntimeError('Native story content validation failed for '+beat+': '+str(result))
        if isinstance(result, tuple) and (False in result or any(isinstance(v, str) and v for v in result)):
            raise RuntimeError('Native story content validation failed for '+beat+': '+str(result))
        api.save(sequences[beat])
        ctx['_report']['scenes'][beat]['native_content_validation'] = True
    return dict(sequences=sequences, scenes=scenes, cast=cast, cast_definitions=definitions,
                gates=gates, report=ctx['_report'])
