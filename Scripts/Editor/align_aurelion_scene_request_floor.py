"""Sample one documented scene station's floor before spawning its owned request.

Returns a proposed location and primitive evidence only. No actor, collision,
asset or runtime state is changed by this module, including at import time.
"""
import math
import unreal

DOCUMENTED_ALIGNMENT = {
    'SurvivorsClearAndQuarantine': {
        'map': '/Game/Aurelion/Maps/L_Aurelion_M12',
        'reason': 'SceneSurfaceAssembly-20260907-170205-48d73976: fixed-height body floated above the descending exit ramp',
        'xy': (180., 23330.),
    },
}
VISUAL_HALF_HEIGHT = 65.0  # Owned Cube at scaleZ1.3; native Body half-height remains45.
TRACE_HALF_RANGE = 250.0


def _xyz(vector):
    result = [float(vector.x), float(vector.y), float(vector.z)]
    if not all(math.isfinite(value) for value in result):
        raise RuntimeError('Non-finite scene request floor geometry')
    return result


def _hits(result):
    if result is None:
        return []
    if isinstance(result, unreal.Array):
        values = list(result)
    elif isinstance(result, (tuple, list)):
        if all(isinstance(value, unreal.HitResult) for value in result):
            values = list(result)
        else:
            arrays = [value for value in result if isinstance(value, (unreal.Array, list))]
            if len(arrays) != 1:
                raise RuntimeError('Unexpected native multi-object floor trace result')
            values = list(arrays[0])
    else:
        raise RuntimeError('Unexpected native multi-object floor trace result')
    if not all(isinstance(value, unreal.HitResult) for value in values):
        raise RuntimeError('Floor trace did not return native HitResults')
    return values


def sample_scene_request_floor(world, beat, proposed, evidence):
    """Return fixedXY/floor+65 for the explicit PlayScene placement correction.

    Root's normal scene request construction is the sole caller. The existing
    final body/five-support/capsule/nav/Visibility validator must still pass.
    """
    row = dict(beat=str(beat), requested=list(proposed), status='checking', samples=[],
               visual_half_height=VISUAL_HALF_HEIGHT, native_body_half_height=45., source_or_actor_writes=False)
    evidence.append(row)
    try:
        if beat not in DOCUMENTED_ALIGNMENT:
            raise RuntimeError('No documented scene request floor correction for '+str(beat))
        policy = DOCUMENTED_ALIGNMENT[beat]
        row['reason'] = policy['reason']
        current = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if (current != world or editor.is_in_play_in_editor()
                or world.get_path_name().split('.')[0] != policy['map']):
            raise RuntimeError('Floor correction requires the exact stopped owned scene world')
        if (len(proposed) != 3 or not all(math.isfinite(float(v)) for v in proposed)
                or math.dist(proposed[:2], policy['xy']) > .01):
            raise RuntimeError('Floor correction must preserve the documented station XY')
        start = unreal.Vector(proposed[0], proposed[1], proposed[2]+TRACE_HALF_RANGE)
        end = unreal.Vector(proposed[0], proposed[1], proposed[2]-TRACE_HALF_RANGE)
        row['trace_start'], row['trace_end'] = _xyz(start), _xyz(end)
        # ObjectTypeQuery1 is the installed WorldStatic mapping used by the actual
        # Meeting floor probe. No static guard/floor/scene actor is ignored.
        hits = _hits(unreal.SystemLibrary.line_trace_multi_for_objects(world, start, end,
            [unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY1], False, [], unreal.DrawDebugTrace.NONE, True))
        floors = []
        for hit in hits:
            fields = hit.to_tuple()
            if not isinstance(fields, tuple) or len(fields) < 8:
                raise RuntimeError('Unexpected native floor HitResult layout')
            point, normal = _xyz(fields[5]), _xyz(fields[7])
            sample = dict(blocking=bool(fields[0]), penetrating=bool(fields[1]), point=point,
                          normal=normal, detail=hit.export_text())
            row['samples'].append(sample)
            if sample['penetrating']:
                raise RuntimeError('Bounded floor trace begins inside static geometry')
            if sample['blocking'] and normal[2] >= .7:
                if (math.dist(point[:2], proposed[:2]) > .1
                        or abs(point[2]-proposed[2]) > TRACE_HALF_RANGE+.1):
                    raise RuntimeError('Physical floor hit escaped the fixed bounded vertical trace')
                floors.append(sample)
        if not floors:
            raise RuntimeError('No walkable static floor within250cm of the authored scene request')
        levels = sorted(sample['point'][2] for sample in floors)
        if levels[-1]-levels[0] > .5:
            raise RuntimeError('Ambiguous multiple walkable static floors in the bounded scene request column')
        floor = max(floors, key=lambda sample: sample['point'][2])
        location = (float(proposed[0]), float(proposed[1]), float(floor['point'][2])+VISUAL_HALF_HEIGHT)
        row.update(status='sampled_fixed_xy_scene_request_floor', floor=floor, result=list(location),
                   z_change=location[2]-float(proposed[2]), matching_floor_hits=len(floors),
                   physical_body_and_corner_validation_pending=True)
        return location
    except Exception as error:
        row['status'], row['error'] = 'failed', str(error)
        raise
