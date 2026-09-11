"""Read-only stopped-editor geometry admission for all owned Aurelion scene requests.

Call after final navigation generation, before saving the current mission map.
This validates static request surfaces and representative CDO approaches; it does
not grant scene admission, modify placements, or simulate campaign prerequisites.
"""
import math
import unreal

MAPS = {12: '/Game/Aurelion/Maps/L_Aurelion_M12', 13: '/Game/Aurelion/Maps/L_Aurelion_M13'}
MISSIONS = {12: 'M12_FireAndFrost', 13: 'M13_ContraryWitness'}
COUNTS = {12: 8, 13: 10}
MAX_BODY_FLOOR_GAP = 30.0  # Existing stations place the 45cm half-body 20cm above their floor.


def _xyz(value):
    result = [float(value.x), float(value.y), float(value.z)]
    if not all(math.isfinite(v) for v in result):
        raise RuntimeError('Non-finite geometry value')
    return result


def _vector(value):
    return unreal.Vector(*value)


def _world(context):
    current = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if (context['chapter'] not in MAPS or current != context['world']
            or unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
            or current.get_path_name().split('.')[0] != MAPS[context['chapter']]):
        raise RuntimeError('Request validation requires the exact stopped owned Aurelion wrapper')
    return current


def _hit(result):
    # UE Python bool-plus-out returns None on a miss and HitResult on success.
    if result is None:
        return dict(blocking=False)
    values = result if isinstance(result, tuple) else (result,)
    hits = [item for item in values if isinstance(item, unreal.HitResult)]
    if len(hits) != 1:
        raise RuntimeError('Unexpected native trace result: '+str(result))
    hit = hits[0]
    fields = hit.to_tuple()
    if not isinstance(fields, tuple) or len(fields) < 8:
        raise RuntimeError('Unexpected native HitResult breaker')
    return dict(blocking=bool(fields[0]), penetrating=bool(fields[1]),
                point=_xyz(fields[5]), normal=_xyz(fields[7]), detail=hit.export_text())


def _ignored_cast(world):
    ignored = []
    queue = list(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.NarrativeCharacter))
    while queue:
        actor = queue.pop()
        if actor in ignored:
            continue
        ignored.append(actor)
        queue.extend(actor.get_attached_actors())
        if isinstance(actor, unreal.NarrativeCharacter):
            visual = actor.get_character_visual()
            if visual is not None:
                queue.append(visual)
    return ignored


def _floor(world, position, ignored, above, below):
    start = [position[0], position[1], position[2]+above]
    end = [position[0], position[1], position[2]-below]
    result = _hit(unreal.SystemLibrary.line_trace_single_by_profile(world, _vector(start), _vector(end),
        unreal.Name('Pawn'), False, ignored, unreal.DrawDebugTrace.NONE, True))
    result['start'], result['end'] = start, end
    result['walkable'] = result['blocking'] and not result.get('penetrating') and result['normal'][2] >= .7
    return result


def _body(world, actor, ignored_cast):
    body = actor.body
    center = _xyz(body.get_world_location())
    extent = _xyz(body.get_scaled_box_extent())
    rotation = body.get_world_rotation()
    if (not all(math.isfinite(float(v)) for v in (rotation.pitch, rotation.yaw, rotation.roll))
            or abs(rotation.pitch) > .01 or abs(rotation.roll) > .01 or min(extent) <= 0):
        raise RuntimeError('Request body must have finite positive upright box dimensions')
    ignored = ignored_cast + [actor]
    profile = body.get_collision_profile_name()
    collision = str(body.get_collision_enabled())
    if body.get_collision_enabled() != unreal.CollisionEnabled.QUERY_AND_PHYSICS:
        raise RuntimeError('Request body lost its actual query/physics collision')
    result = dict(center=center, extent=extent, rotation=rotation.export_text(), profile=str(profile),
                  collision=collision, floor_samples=[])
    result['intrusion'] = _hit(unreal.SystemLibrary.box_trace_single_by_profile(world, _vector(center),
        _vector([center[0], center[1], center[2]+.01]), _vector(extent), rotation, profile, False,
        ignored, unreal.DrawDebugTrace.NONE, True))
    yaw = math.radians(float(rotation.yaw))
    # Center and all four actual box corners, inset by 1cm to avoid edge ambiguity.
    for x, y in [(0., 0.)] + [(sx*max(0., extent[0]-1.), sy*max(0., extent[1]-1.))
                               for sx in (-1., 1.) for sy in (-1., 1.)]:
        bottom = [center[0]+x*math.cos(yaw)-y*math.sin(yaw),
                  center[1]+x*math.sin(yaw)+y*math.cos(yaw), center[2]-extent[2]]
        floor = _floor(world, bottom, ignored, 1., MAX_BODY_FLOOR_GAP+1.)
        floor['body_bottom'] = bottom
        floor['gap'] = bottom[2]-floor['point'][2] if floor['blocking'] else None
        floor['supported'] = bool(floor['walkable'] and -.1 <= floor['gap'] <= MAX_BODY_FLOOR_GAP)
        result['floor_samples'].append(floor)
    result['clear_and_supported'] = not result['intrusion']['blocking'] and all(row['supported'] for row in result['floor_samples'])
    return result


def _approach(world, actor, scene, hero, ignored_cast, candidate):
    row = dict(requested=candidate, accepted=False)
    source = _xyz(scene.get_actor_location())
    # Use the guarded native wrapper once. Never ProcessEvent on NavigationSystemV1's CDO.
    path = unreal.SovAurelionNavigationLibrary.find_path_to_location_synchronously(
        world, _vector(source), _vector(candidate), None, None)
    row['path'] = dict(source=source, destination=candidate,
        complete=bool(path is not None and path.is_valid() and not path.is_partial()),
        points=[_xyz(point) for point in path.path_points] if path is not None else [])
    if not row['path']['complete'] or len(row['path']['points']) < 2:
        row['rejection'] = 'Missing or partial local path from the scene hero mark'
        return row
    for name, actual, requested in (('source', row['path']['points'][0], source),
                                    ('destination', row['path']['points'][-1], candidate)):
        within = math.dist(actual[:2], requested[:2]) <= 40. and abs(actual[2]-requested[2]) <= 200.
        row['path'][name+'_within_bounds'] = within
        if not within:
            row['rejection'] = 'Full path '+name+' escaped the bounded40cmXY/200cmZ projection'
            return row
    projected = row['path']['points'][-1]
    if not row['path']['source_within_bounds'] or not row['path']['destination_within_bounds']:
        row['rejection'] = 'Both full-path endpoints must retain their bounded source/candidate'
        return row
    row['nav_projection'] = projected
    floor = _floor(world, [projected[0], projected[1], candidate[2]], ignored_cast, hero['half_height']+100., 350.)
    row['floor'] = floor
    if not floor['walkable']:
        row['rejection'] = 'No supported walkable approach floor'
        return row
    center = [projected[0], projected[1], floor['point'][2]+hero['half_height']+2.]
    row['center'] = center
    request_center = _xyz(actor.get_actor_location())
    row['request_distance'] = math.dist(center, request_center)
    if row['request_distance'] > float(actor.interactable.interaction_distance):
        row['rejection'] = 'Outside actual native request range'
        return row
    if abs(projected[2]-floor['point'][2]) > 30.:
        row['rejection'] = 'Navigation projection is on a different floor surface'
        return row
    transform = unreal.Transform(location=_vector(center), rotation=unreal.Rotator(pitch=0., yaw=0., roll=0.),
                                 scale=unreal.Vector(1., 1., 1.))
    capsule = unreal.SovAurelionSceneValidationLibrary.validate_aurelion_exit_geometry(
        world, transform, hero['radius'], hero['half_height'])
    row['capsule'] = dict(succeeded=bool(capsule.succeeded), clear=bool(capsule.clear),
                         recovery_excluded=bool(capsule.recovery_excluded), blockers=list(capsule.blocking_components))
    if not capsule.succeeded or not capsule.clear:
        row['rejection'] = 'Actual hero CDO capsule is obstructed or recovery-excluded'
        return row
    eyes = [center[0], center[1], center[2]+hero['base_eye_height']]
    row['native_eye_origin'], row['native_eye_target'] = eyes, request_center
    row['visibility'] = _hit(unreal.SystemLibrary.line_trace_single(world, _vector(eyes), _vector(request_center),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, ignored_cast+[actor], unreal.DrawDebugTrace.NONE, True))
    if row['visibility']['blocking']:
        row['rejection'] = 'Static obstruction in native pawn-eye Visibility segment'
        return row
    row['accepted'] = True
    return row


def validate_scene_request_surfaces(context, scene_report):
    """Populate scene_report in place before checks so root preserves failures.

    The caller must retain its normal source-package guards and invoke this only
    after the final navigation build, before Editor.save_current_level().
    """
    world = _world(context)
    output = dict(status='checking', scope='Static request bodies, floors, local full nav paths and CDO pawn-eye Visibility',
        actual_gameplay_admission=False, runtime_capsule_resize_qualified=False, source_or_actor_writes=False,
        max_body_floor_gap=MAX_BODY_FLOOR_GAP, scenes=[], failures=[])
    scene_report['request_surface_geometry'] = output
    if unreal.SovAurelionNavigationLibrary.is_navigation_being_built_or_locked(world):
        output['status'] = 'pending_navigation_build'
        raise RuntimeError('Request surface validation cannot pass while navigation is pending')
    expected = scene_report['scenes']
    if len(expected) != COUNTS[context['chapter']]:
        raise RuntimeError('Scene surface census must cover every authored scene in this map')
    actors = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.SovAurelionRequestActor)
              if a.operation == unreal.SovAurelionRequest.PLAY_SCENE]
    if len(actors) != len(expected) or {str(a.beat_id) for a in actors} != set(expected):
        raise RuntimeError('PlayScene request identities do not exactly match the full scene census')
    ignored = _ignored_cast(world)
    output['ignored_mobile_cast_and_attachments'] = [a.get_path_name() for a in ignored]
    for actor in sorted(actors, key=lambda value: str(value.beat_id)):
        _world(context)
        beat = str(actor.beat_id)
        row = dict(beat=beat, actor=actor.get_path_name(), approaches=[])
        output['scenes'].append(row)
        try:
            if str(actor.mission_id) != MISSIONS[context['chapter']] or 'Sov.Aurelion.WorkPC.20260907' not in [str(t) for t in actor.tags]:
                raise RuntimeError('Scene request is not the exact owned mission instance')
            scene = actor.story
            if scene is None or scene.get_path_name() != expected[beat]['actor'] or scene.get_world() != world:
                raise RuntimeError('Request points to a different scene actor/world')
            if not (0 < float(actor.interactable.interaction_distance) <= 300.):
                raise RuntimeError('Invalid native request interaction distance')
            hero_name = expected[beat]['hero']
            hero_class = context['heroes'][hero_name]['pawn']
            cdo = unreal.get_default_object(hero_class)
            capsule = cdo.get_editor_property('capsule_component')
            hero = dict(name=hero_name, class_path=hero_class.get_path_name(), radius=float(capsule.get_scaled_capsule_radius()),
                        half_height=float(capsule.get_scaled_capsule_half_height()),
                        base_eye_height=float(cdo.get_editor_property('base_eye_height')))
            if not (0 < hero['radius'] <= hero['half_height'] <= 400. and math.isfinite(hero['base_eye_height'])):
                raise RuntimeError('Invalid representative hero CDO geometry')
            row['hero'] = hero
            row['body'] = _body(world, actor, ignored)
            center = _xyz(actor.get_actor_location())
            station = _xyz(scene.get_actor_location())
            start_angle = math.atan2(station[1]-center[1], station[0]-center[0])
            # Deterministic validation samples; these positions are never assigned.
            for distance in (160., 220.):
                for turn in (0., 45., -45., 90., -90., 135., -135., 180.):
                    angle = start_angle+math.radians(turn)
                    candidate = [center[0]+distance*math.cos(angle), center[1]+distance*math.sin(angle), center[2]]
                    approach = _approach(world, actor, scene, hero, ignored, candidate)
                    row['approaches'].append(approach)
                    if approach['accepted']:
                        break
                if row['approaches'][-1]['accepted']:
                    break
            row['passed'] = row['body']['clear_and_supported'] and any(a['accepted'] for a in row['approaches'])
            if not row['passed']:
                row['error'] = 'Request body/floor or every bounded reachable visible approach failed'
        except Exception as error:
            row['passed'], row['error'] = False, str(error)
        if not row['passed']:
            output['failures'].append(beat+': '+row['error'])
    _world(context)
    output['checked'] = len(output['scenes'])
    output['status'] = 'passed_static_request_surfaces' if not output['failures'] else 'failed_static_request_surfaces'
    if output['failures']:
        raise RuntimeError('Scene request surface validation failed: '+'; '.join(output['failures']))
    return output
