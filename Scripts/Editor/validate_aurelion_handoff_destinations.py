"""Read-only stopped-editor projection/static-capsule gate for six owned handoffs.
No import-time engine calls, asset loads, placement changes, or campaign actions.
"""
import math
import unreal

MAPS={12:'/Game/Aurelion/Maps/L_Aurelion_M12',13:'/Game/Aurelion/Maps/L_Aurelion_M13'}
MISSIONS={12:'M12_FireAndFrost',13:'M13_ContraryWitness'}
EXPECTED={
    12:{'M12_SeleneEntry':('HandoffToSelene','Selene'),
        'M12_TarrikSharedBreach':('HandoffToTarrikRescue','Tarrik'),
        'M12_SeleneCage':('HandoffToSeleneCage','Selene'),
        'M12_TarrikCrucible':('HandoffToTarrikCrucible','Tarrik')},
    13:{'M13_SeleneAssent':('HandoffToSeleneAssent','Selene'),
        'M13_TarrikAftermath':('HandoffToTarrikAftermath','Tarrik')},
}


def _valid(obj): return obj is not None and unreal.SystemLibrary.is_valid(obj)
def _path(obj): return obj.get_path_name() if _valid(obj) else None
def _tag(value): return str(unreal.GameplayTagLibrary.get_tag_name(value))


def _xyz(v):
    values=[float(v.x),float(v.y),float(v.z)]
    if not all(math.isfinite(n) for n in values): raise RuntimeError('Nonfinite handoff destination geometry')
    return values


def validate_handoff_destinations(context, output):
    """Write primitive evidence to the caller report, raising before map save on failure."""
    if not isinstance(output,dict): raise TypeError('Pass the authoring report row')
    output.update(status='checking',anchors=[],failures=[],runtime_handoff_verified=False,
        scope='Exact native projection plus static destination capsule geometry; mission progress, dynamic cast, spawn adjustment and actual handoff remain runtime gates.')
    try:
        chapter=context['chapter'];world=context['world']
        actual=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        if chapter not in MAPS or not _valid(world) or actual!=world or unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
            raise RuntimeError('Handoff geometry requires the exact current stopped editor world')
        if world.get_path_name().split('.')[0]!=MAPS[chapter]: raise RuntimeError('Handoff geometry is restricted to the owned chapter map')
        if unreal.SovAurelionNavigationLibrary.is_navigation_being_built_or_locked(world):
            raise RuntimeError('Navigation is still building; handoff geometry cannot pass yet')
        mode_class=world.get_world_settings().get_editor_property('default_game_mode')
        mode=unreal.get_default_object(mode_class) if mode_class is not None else None
        if not isinstance(mode,unreal.SovCampaignGameMode): raise RuntimeError('Current map lacks its native campaign GameMode')
        mission=mode.get_editor_property('initial_mission')
        if not _valid(mission) or str(mission.mission_id)!=MISSIONS[chapter]: raise RuntimeError('Current map mission identity differs')
        all_anchors=list(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SovCampaignHandoffAnchor))
        if len(all_anchors)!=len(EXPECTED[chapter]): raise RuntimeError('Unexpected chapter handoff actor census')
        found={}
        for actor in all_anchors:
            key=str(actor.anchor_id)
            if str(actor.mission_id)!=MISSIONS[chapter] or key not in EXPECTED[chapter] or key in found:
                raise RuntimeError('Missing, duplicate or foreign chapter handoff identity: '+key)
            found[key]=actor
        requests=list(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SovAurelionRequestActor))
        output.update(chapter=chapter,map=MAPS[chapter],mission=_path(mission),expected_count=len(EXPECTED[chapter]))
        for key,(beat_id,hero) in EXPECTED[chapter].items():
            actor=found[key];destination=actor.destination
            row=dict(anchor_id=key,actor=_path(actor),beat=beat_id,hero=hero,passed=False)
            output['anchors'].append(row)
            if str(actor.handoff_beat)!=beat_id or not _valid(destination): raise RuntimeError('Authored handoff destination/beat mismatch: '+key)
            beats=[b for b in mission.beats if str(b.beat_id)==beat_id]
            if len(beats)!=1 or str(beats[0].required_handoff_anchor_id)!=key:
                raise RuntimeError('Mission handoff beat is missing or bound to a different anchor: '+key)
            lead='Sov.Character.Player.'+hero
            if _tag(beats[0].handoff_to_protagonist)!=lead: raise RuntimeError('Destination protagonist does not match authored handoff: '+key)
            if _tag(mission.protagonist)==lead:
                selected_class=mission.pawn_class
            else:
                profiles=[p for p in mission.alternate_protagonists if _tag(p.protagonist)==lead]
                if len(profiles)!=1: raise RuntimeError('Missing/ambiguous destination protagonist profile: '+key)
                selected_class=profiles[0].pawn_class
            cls=context['heroes'][hero]['pawn']
            if not _valid(cls) or selected_class!=cls: raise RuntimeError('Mission destination class differs from the already loaded authored hero class: '+key)
            defaults=unreal.get_default_object(cls)
            expected_type=unreal.SovSeleneCharacter if hero=='Selene' else unreal.SovTarrikCharacter
            if not isinstance(defaults,expected_type): raise RuntimeError('Destination hero native type mismatch: '+key)
            capsule=defaults.get_editor_property('capsule_component')
            radius=float(capsule.get_scaled_capsule_radius());half=float(capsule.get_scaled_capsule_half_height())
            if not math.isfinite(radius) or not math.isfinite(half) or not 0<radius<=half<=400.:
                raise RuntimeError('Invalid destination CDO capsule: '+key)
            bound=[r for r in requests if r.handoff_anchor==actor]
            if len(bound)!=1 or bound[0].operation!=unreal.SovAurelionRequest.HANDOFF or str(bound[0].mission_id)!=MISSIONS[chapter] or str(bound[0].beat_id)!=beat_id:
                raise RuntimeError('Expected one exact ordinary request bound to handoff: '+key)
            before=(actor.get_actor_transform().export_text(),destination.get_world_transform().export_text())
            center=_xyz(destination.get_world_location());feet=[center[0],center[1],center[2]-half]
            row.update(request=_path(bound[0]),destination_component=_path(destination),authored_transform=before[1],
                authored_center=center,feet=feet,query_extent=[60.,60.,120.],maximum_projection_distance=120.,
                hero_class=_path(cls),hero_defaults=_path(defaults),capsule=_path(capsule),radius=radius,half_height=half)
            # Matches ASovCampaignHandoffAnchor::ValidateRequest. This static wrapper gets the current World/NavSys.
            value=unreal.NavigationSystemV1.project_point_to_navigation(world,unreal.Vector(*feet),None,None,unreal.Vector(60.,60.,120.))
            projected=value if isinstance(value,unreal.Vector) else next((v for v in value if isinstance(v,unreal.Vector)),None) if isinstance(value,tuple) else None
            success=projected is not None and not (isinstance(value,tuple) and any(v is False for v in value))
            row['projection_succeeded']=success
            if not success: raise RuntimeError('Handoff feet have no native nearby navigation projection: '+key)
            point=_xyz(projected);distance=math.dist(feet,point)
            row.update(projected_feet=point,projection_distance=distance)
            if distance>120.: raise RuntimeError('Handoff feet projection exceeds native120cm distance: '+key)
            projected_center=[point[0],point[1],point[2]+half]
            # Native handoff keeps destination rotation and explicitly normalizes scale to one.
            transform=unreal.Transform(location=unreal.Vector(*projected_center),rotation=destination.get_world_rotation(),scale=unreal.Vector(1.,1.,1.))
            geometry=unreal.SovAurelionSceneValidationLibrary.validate_aurelion_exit_geometry(world,transform,radius,half)
            row.update(projected_center=projected_center,projected_transform=transform.export_text(),
                static_geometry_query_succeeded=bool(geometry.succeeded),blocking_components=list(geometry.blocking_components),
                recovery_exclusion_reported=bool(geometry.recovery_excluded),static_geometry_report=str(geometry.report),
                ignored_characters=list(geometry.ignored_characters))
            # This gate needs the helper's exact ECC_Pawn static overlap result. Its recovery-exclusion policy is
            # additional metadata, not a handoff admission rule; the native handoff itself does not consult it.
            if not geometry.succeeded or row['blocking_components']:
                raise RuntimeError('Projected handoff destination capsule is statically obstructed: '+key+' '+str(row['blocking_components']))
            if before!=(actor.get_actor_transform().export_text(),destination.get_world_transform().export_text()):
                raise RuntimeError('Handoff changed during synchronous geometry validation: '+key)
            row.update(passed=True,authored_transform_unchanged=True)
        output.update(status='passed',checked=len(output['anchors']))
        return output
    except Exception as error:
        output.update(status='failed',error=str(error))
        output['failures'].append(str(error))
        raise
