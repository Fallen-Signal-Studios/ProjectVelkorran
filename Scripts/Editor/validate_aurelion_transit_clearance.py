"""Read-only stopped-Editor rescue door geometry gate. No import-time engine calls."""
import json
import math
import unreal


def _xyz(v): return [float(v.x),float(v.y),float(v.z)]


def _path(obj):
    return obj.get_path_name() if obj is not None and unreal.SystemLibrary.is_valid(obj) else None


def _hit(value):
    if value is None: return dict(blocking=False,native_return='None')
    hits=[v for v in (value if isinstance(value,tuple) else (value,)) if isinstance(v,unreal.HitResult)]
    if len(hits)!=1: raise RuntimeError('Unexpected native HitResult wrapper')
    parts=hits[0].to_tuple()
    return dict(blocking=bool(parts[0]),initial_overlap=bool(parts[1]),actor=_path(parts[9]),component=_path(parts[10]),
        impact=_xyz(parts[5]),raw=hits[0].export_text())


def validate_rescue_door_clearance(world, output):
    """Populate the caller's primitive report row; reject before save on any blocked sweep."""
    if not isinstance(output,dict): raise TypeError('Pass the authoring report dictionary for this gate')
    output.update(status='checking',sweeps=[],floors=[],method='Actual named-profile native box sweeps, not runtime transit execution')
    try:
        editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        actual=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        if editor.is_in_play_in_editor() or world!=actual or _path(world) is None:
            raise RuntimeError('Rescue door validation requires the current stopped editor world')
        if world.get_path_name().split('.')[0]!='/Game/Aurelion/Maps/L_Aurelion_M12':
            raise RuntimeError('Rescue door gate is scoped to the owned M12 map')
        matches=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SovWorldTransitActor)
            if str(a.transit_id)=='M12_E3_RescueApproach']
        if len(matches)!=1: raise RuntimeError('Missing or duplicate rescue door identity')
        door=matches[0];body=door.moving_body
        if door.kind!=unreal.SovWorldTransitKind.DOOR: raise RuntimeError('Rescue mechanism is not an ordinary Door')
        center=_xyz(body.get_world_location());extent=_xyz(body.get_scaled_box_extent());rotation=body.get_world_rotation()
        destination=_xyz(unreal.MathLibrary.transform_location(door.scene_root.get_world_transform(),door.destination_offset))
        if not all(math.isfinite(v) for v in center+extent+destination) or min(extent)<=0:
            raise RuntimeError('Nonfinite/empty door geometry')
        if abs(rotation.pitch)>.001 or abs(rotation.roll)>.001 or math.dist(center,destination)<1.:
            raise RuntimeError('Expected an upright door with a nonzero physical route')
        profile=body.get_collision_profile_name()
        if str(profile)!='BlockAllDynamic' or body.get_collision_object_type()!=unreal.CollisionChannel.cast(1):
            raise RuntimeError('Expected the original WorldDynamic BlockAllDynamic collision contract')
        mesh=door.visual.get_editor_property('static_mesh')
        if _path(mesh)!='/Engine/BasicShapes/Cube.Cube': raise RuntimeError('Rescue door visual must match the authored unit Cube')
        visual=_xyz(door.visual.get_world_scale())
        if any(abs(visual[i]*50.-extent[i])>.01 for i in range(3)):
            raise RuntimeError('Rescue door visual dimensions differ from the actual moving box')
        before=(door.get_actor_transform().export_text(),body.get_world_transform().export_text(),extent,
            door.visual.get_editor_property('relative_scale3d').export_text(),door.destination_offset.export_text())
        output.update(door=_path(door),body=_path(body),center=center,destination=destination,extent=extent,
            orientation=rotation.export_text(),visual_world_scale=visual,profile=str(profile),ignored_actors=[_path(door)])
        for name,a,b in [('initial',center,[center[0],center[1],center[2]+.1]),('full_open',center,destination),('full_close',destination,center)]:
            hit=_hit(unreal.SystemLibrary.box_trace_single_by_profile(world,unreal.Vector(*a),unreal.Vector(*b),unreal.Vector(*extent),
                rotation,profile,False,[door],unreal.DrawDebugTrace.NONE,True))
            output['sweeps'].append(dict(name=name,start=a,end=b,hit=hit))
            if hit['blocking']: raise RuntimeError('Rescue door '+name+' sweep blocked: '+json.dumps(hit))
        yaw=math.radians(rotation.yaw)
        for lx,ly in [(0.,0.),(-extent[0],-extent[1]),(-extent[0],extent[1]),(extent[0],-extent[1]),(extent[0],extent[1])]:
            x=center[0]+lx*math.cos(yaw)-ly*math.sin(yaw);y=center[1]+lx*math.sin(yaw)+ly*math.cos(yaw)
            hit=_hit(unreal.SystemLibrary.line_trace_single_for_objects(world,unreal.Vector(x,y,center[2]+200.),
                unreal.Vector(x,y,center[2]-extent[2]-200.),[unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY1],False,
                [door],unreal.DrawDebugTrace.NONE,True))
            gap=center[2]-extent[2]-hit['impact'][2] if hit['blocking'] else None
            output['floors'].append(dict(xy=[x,y],hit=hit,bottom_gap_cm=gap))
            if not hit['blocking'] or hit['initial_overlap'] or gap<.5 or gap>5.:
                raise RuntimeError('Rescue door lacks the measured small clear floor gap')
        after=(door.get_actor_transform().export_text(),body.get_world_transform().export_text(),_xyz(body.get_scaled_box_extent()),
            door.visual.get_editor_property('relative_scale3d').export_text(),door.destination_offset.export_text())
        if before!=after: raise RuntimeError('Door changed during synchronous read-only authoring validation')
        output.update(status='passed',door_unchanged=True,ordinary_transit_and_wave_publication_verified=False)
        return output
    except Exception as error:
        output.update(status='failed',error=str(error))
        raise
