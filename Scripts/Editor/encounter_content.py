"""Author the four M12 encounter rosters in the owned Aurelion map.

Import only; build_encounters(ctx) edits the current stopped editor world but does
not load/save maps, run gameplay, call proof APIs, spawn a second cast, or build nav.
Root owns the map transaction and all missions, scenes, checkpoints and handoffs.
The returned objects are wiring references, never simulated completion evidence.
"""
import math
from pathlib import Path
import sys
import unreal

STAMP = 'Sov.Aurelion.EncounterContent.20260907'
MISSION = 'M12_FireAndFrost'
ENCOUNTERS = {
    'E1': 'M12_E1_PressureHall', 'E2': 'M12_E2_RelayOverlook',
    'E3': 'M12_E3_SharedBreach', 'E4A': 'M12_E4_QuarantineCrucibleA',
    'E4B': 'M12_E4_QuarantineCrucibleB',
}
# Last coordinate is the verified supporting floor, not the graybox marker's pivot.
ROSTERS = {
    'E1': [
        ('Drone1','SecurityDrone',(-7500,-14400,0),0,'MARKSMAN'),
        ('Drone2','SecurityDrone',(-6500,-14300,0),0,'COMMANDER'),
        ('Drone3','SecurityDrone',(-7500,-12800,0),0,'MARKSMAN'),
        ('Drone4','SecurityDrone',(-6500,-12800,0),0,'MARKSMAN'),
        ('Drone5','SecurityDrone',(-7300,-12300,0),1,'MARKSMAN'),
        ('Drone6','SecurityDrone',(-6700,-12300,0),1,'MARKSMAN'),
    ],
    'E2': [
        ('Enforcer1','Enforcer',(6200,-10600,0),0,'LINE'),
        ('Enforcer2','Enforcer',(7000,-10400,0),0,'LINE'),
        # The source marker at 7500,-10000 intersects the balcony descent ramp.
        ('Enforcer3','Enforcer',(6900,-9800,0),0,'LINE'),
        ('Enforcer4','Enforcer',(7900,-10700,0),0,'LINE'),
        ('Drone1','ContaminatedDrone',(8500,-11400,0),0,'MARKSMAN'),
        ('Drone2','ContaminatedDrone',(9400,-9900,300),0,'MARKSMAN'),
    ],
    'E3': [
        ('Linkbound1','Linkbound',(-800,8500,-600),0,'LINE'),
        ('Linkbound2','Linkbound',(-400,9700,-600),0,'LINE'),
        ('Linkbound3','Linkbound',(500,10000,-600),0,'LINE'),
        ('WallRunner','WallRunner',(-1300,9300,-600),0,'DUELIST'),
        ('Linkbound4','Linkbound',(-800,10800,-600),1,'LINE'),
        ('Linkbound5','Linkbound',(700,10900,-600),1,'LINE'),
        ('Weaver','Weaver',(0,11000,-600),1,'CONTROLLER'),
    ],
    'E4': [
        ('Linkbound1','Linkbound',(-700,20800,-1200),0,'LINE'),
        ('Linkbound2','Linkbound',(700,20800,-1200),0,'LINE'),
        ('Weaver','Weaver',(300,21700,-1200),0,'CONTROLLER'),
        ('Elite','Elite',(0,21800,-1200),0,'BRUTE'),
        ('WallRunner','WallRunner',(1500,21800,-1200),1,'DUELIST'),
    ],
}
PROTECTED = {
    'E3': ('TrappedMarine','Lyric'),
    # E3's two actors retain their completed director identity. No dual registration.
    'E4': ('Malik','Tharne','Lyessa','WestDominionStretcher',
           'WestReformationStretcher','EastDominionWalker','EastReformationWalker'),
}
ENTRY = {
    'E1': ((-7000,-17400,110),(1120,250,210),'PressureHall'),
    'E2': ((7000,-12700,110),(2950,230,210),'RelayOverlook'),
    'E3': ((0,6800,-490),(1450,260,210),'BreachSharedJunction'),
    'E4A': ((0,18900,-1090),(3200,240,210),'SeverCrucibleLinks'),
    'E4B': ((-250,19600,-1090),(500,400,210),'ThermalFracture'),
}


def _path(obj):
    return obj.get_path_name() if obj else None


def build_encounters(ctx):
    """ctx: chapter/world/api/hero profiles/existing/enemy_definitions/cast.

    api exposes spawn, prop, required, vector, rot, text and BASE. No root-only
    helper is invoked implicitly. Module-owned labels/tags are checked on rerun.
    Enemy builder values are exact UObject paths, cast values are actual NPCs.
    """
    result = dict(directors={}, objectives={}, hostiles={}, protected={}, receivers=[],
                  thermal=None, frost_anchor=None, priority={}, support=None,
                  handoff_destinations={}, entry_specs=ENTRY, wall_routes=[],
                  support_specs={}, scanner_specs=[], request_controls=[],
                  floor_checks=[],
                  authored_only=True, requires_live_validation=[])
    if ctx['chapter'] == 13:
        return result
    if ctx['chapter'] != 12:
        raise RuntimeError('Encounter module only admits M12 or the empty M13 branch')
    if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
        raise RuntimeError('Stop PIE before encounter authoring')
    api, world = ctx['api'], ctx['world']
    current = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    package = world.get_path_name().split('.')[0]
    if current != world or package != api.BASE+'Maps/L_Aurelion_M12':
        raise RuntimeError('Encounter authoring requires the owned full M12 map')
    definitions, cast = ctx['enemy_definitions'], ctx['cast']
    if any(actor.get_world() != world for actor in ctx['existing'].values()):
        raise RuntimeError('Existing geometry includes an actor outside the owned current map')
    missing = sorted({row[1] for rows in ROSTERS.values() for row in rows}-set(definitions))
    if missing: raise RuntimeError('Enemy builder roles missing: '+repr(missing))
    for key in set(sum((list(keys) for keys in PROTECTED.values()), [])):
        actor = cast.get(key)
        if not isinstance(actor, unreal.SovNPCCharacterBase) or actor.get_world() != world:
            raise RuntimeError('Missing current native cast actor: '+key)
    if len({_path(cast[key]) for keys in PROTECTED.values() for key in keys}) != 9:
        raise RuntimeError('Protected cast identities alias across encounters')
    editor_actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    present = {}
    for actor in editor_actors.get_all_level_actors():
        present.setdefault(actor.get_actor_label(), []).append(actor)
    created = []

    def spawn(cls, name, location, yaw=90, folder='Aurelion/Encounters'):
        label = 'Aurelion_'+name
        previous = present.get(label, [])
        if len(previous) > 1: raise RuntimeError('Duplicate owned actor label: '+label)
        if previous:
            actor = previous[0]
            expected = cls if isinstance(cls, unreal.Class) else cls.static_class()
            if unreal.Name(STAMP) not in actor.tags or actor.get_class() != expected:
                raise RuntimeError('Refusing to replace unowned or wrong-class actor: '+label)
            actor.set_actor_location(api.vector(location), False, False)
            actor.set_actor_rotation(api.rot(yaw), False)
        else:
            actor = api.spawn(cls,name,location,yaw,folder=folder)
            actor.set_editor_property('tags', list(actor.tags)+[unreal.Name(STAMP)])
            present[label] = [actor]
        created.append(actor)
        return actor

    def prop(obj, **values):
        return api.prop(obj, **values)

    def struct(cls, **values):
        return prop(cls(), **values)

    def sign(name, words, location, yaw=-90, size=25):
        a = spawn(unreal.TextRenderActor,name,location,yaw)
        c = a.get_editor_property('text_render')
        c.set_text(api.text('Encounter.'+name, words)); c.set_world_size(size)
        c.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
        return a

    def box(name, position, extent, material='wall', yaw=0, collision=True):
        a = spawn(unreal.StaticMeshActor,name,position,yaw,'Aurelion/Encounters/Geometry')
        m = a.get_editor_property('static_mesh_component')
        m.set_static_mesh(api.required('/Engine/BasicShapes/Cube'))
        m.set_material(0,api.required(api.BASE+'Materials/M_GB_'+material))
        a.set_actor_scale3d(api.vector([x/50.0 for x in extent]))
        m.set_collision_profile_name('BlockAll' if collision else 'NoCollision')
        return a

    classes, dimensions = {}, {}
    for role, value in definitions.items():
        cls = unreal.load_class(None,value['class'])
        if not cls: raise RuntimeError('Missing enemy class: '+value['class'])
        cdo = unreal.get_default_object(cls)
        if not isinstance(cdo, unreal.SovNPCCharacterBase):
            raise RuntimeError('Enemy class lacks native encounter readiness: '+role)
        cap = cdo.get_editor_property('capsule_component')
        h, r = cap.get_scaled_capsule_half_height(), cap.get_scaled_capsule_radius()
        if not all(math.isfinite(v) and v>0 for v in (h,r)):
            raise RuntimeError('Invalid native enemy capsule: '+role)
        classes[role], dimensions[role] = cls, (h,r)

    def hit_result(value):
        if isinstance(value, unreal.HitResult): return value
        if isinstance(value, tuple):
            hits = [v for v in value if isinstance(v, unreal.HitResult)]
            if len(hits) == 1: return hits[0]
        return None

    def checked_floor(identity, floor, half_height, radius, owned_existing=()):
        # Static collision is checked in the actual copied map, including ramps and cover.
        # Ignore all authored characters, then separately retain the exact actor roster.
        ignored = [a for rows in present.values() for a in rows if isinstance(a,unreal.Pawn)] + list(owned_existing)
        hit = hit_result(unreal.SystemLibrary.line_trace_single(world,
            api.vector((floor[0],floor[1],floor[2]+60)),
            api.vector((floor[0],floor[1],floor[2]-80)),
            unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,ignored,unreal.DrawDebugTrace.NONE,True))
        values = hit.to_tuple() if hit else None
        if not values or not values[0]:
            raise RuntimeError('No physical encounter floor: '+identity)
        point,normal = values[5],values[7]
        if normal.z < .7 or abs(point.z-floor[2]) > 3:
            raise RuntimeError('Authored floor differs from reviewed geometry: '+identity+' '+str(point))
        xyz = (floor[0],floor[1],point.z+half_height+2)
        blocked = hit_result(unreal.SystemLibrary.capsule_trace_single(world,api.vector(xyz),
            api.vector((xyz[0],xyz[1],xyz[2]+.5)),max(1,radius-1),max(radius,half_height-2),
            unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,ignored,unreal.DrawDebugTrace.NONE,True))
        if blocked and blocked.to_tuple()[0]:
            raise RuntimeError('Static geometry obstructs the native spawn capsule: '+identity)
        result['floor_checks'].append(dict(participant=identity,floor_z=float(point.z),
            half_height=float(half_height),radius=float(radius),center=xyz,static_capsule_clear=True))
        return xyz

    participants, compositions = {}, {}
    for group, rows in ROSTERS.items():
        participants[group], compositions[group] = [], []
        for suffix, role, floor, wave, decision in rows:
            identity = group+'.'+suffix
            h, radius = dimensions[role]
            xyz = checked_floor(identity,floor,h,radius)
            a = spawn(classes[role],identity,xyz,-90,'Aurelion/Encounters/'+group)
            prop(a,authored_placed_definition=api.required(definitions[role]['definition']),
                 auto_possess_ai=unreal.AutoPossessAI.PLACED_IN_WORLD_OR_SPAWNED)
            result['hostiles'][identity] = a
            participants[group].append(struct(unreal.SovEncounterParticipant,
                participant_id=unreal.Name(identity),character=a,required_for_victory=True,
                allow_mass_representation=False))
            compositions[group].append(struct(unreal.SovEncounterCompositionMember,
                participant_id=unreal.Name(identity),tier=unreal.SovEncounterDecisionTier.COMBATANT,
                role=getattr(unreal.SovEncounterRole,decision),wave=wave))
        for key in PROTECTED.get(group, ()):
            identity = group+'.Protected.'+key
            actor = cast[key]
            participants[group].append(struct(unreal.SovEncounterParticipant,
                participant_id=unreal.Name(identity),character=actor,required_for_victory=False,
                allow_mass_representation=False))
            compositions[group].append(struct(unreal.SovEncounterCompositionMember,
                participant_id=unreal.Name(identity),tier=unreal.SovEncounterDecisionTier.SUPPORTING,
                role=unreal.SovEncounterRole.OBJECTIVE,wave=0))
            result['protected'][identity] = actor
    if len(result['hostiles']) != 24:
        raise RuntimeError('The exact full-layout hostile budget changed')

    def director(key, cls, group=None, maximum=16):
        a = spawn(cls,key+'_Director',ENTRY[key][0],0)
        prop(a,encounter_id=unreal.Name(ENCOUNTERS[key]),
             hold_participants_before_entry=bool(group),hold_non_victory_participants_before_entry=False,
             participants=participants[group] if group else [],
             protected_participant_ids=[unreal.Name(group+'.Protected.'+k) for k in PROTECTED.get(group,())])
        coord = a.get_coordination_component()
        prop(coord,composition=compositions[group] if group else [],
             wave_release_rules=[],maximum_combatants=maximum,maximum_supporting=24,
             melee_attacker_slots=2)
        result['directors'][ENCOUNTERS[key]] = a
        obj = spawn(unreal.SovCampaignEncounterObjective,key+'_Entry',ENTRY[key][0],0)
        prop(obj,encounter_director=a,mission_id=unreal.Name(MISSION),
             completion_beat=unreal.Name(ENTRY[key][2]),start_on_player_overlap=(key!='E4B'),
             retry_initial_entry_while_overlapping=(key!='E4B'),
             required_receivers=[])
        obj.start_volume.set_box_extent(api.vector(ENTRY[key][1]),False)
        result['objectives'][ENTRY[key][2]] = obj
        return a, coord, obj

    e1,c1,_ = director('E1',unreal.SovEncounterDirector,'E1',4)
    e2,c2,e2_obj = director('E2',unreal.SovEncounterDirector,'E2',6)
    e3,c3,_ = director('E3',unreal.SovEncounterDirector,'E3',6)
    phase_a,ca,_ = director('E4A',unreal.SovAurelionLinkPhaseDirector,'E4',5)
    # Initially empty: native phase A transfers the SAME surviving actors, including cast.
    phase_b,cb,phase_b_obj = director('E4B',unreal.SovAurelionThermalPhaseDirector)
    prop(phase_b,elite_participant_id=unreal.Name('E4.Elite'))
    h = result['hostiles']

    # Configure references only. The readiness bootstrap earns its native instance at runtime.
    # Matches the approved graybox command-emitter location on drone 2.
    lead = h['E1.Drone2'].get_formation_link()
    prop(lead,auto_initialize_fresh_link=True,
         linked_actors=[h['E1.Drone'+str(i)] for i in (1,3,4,5,6)])
    for i in (1,3,4,5,6):
        prop(h['E1.Drone'+str(i)].get_formation_link(),auto_initialize_fresh_link=False,linked_actors=[])
    prop(c1,wave_release_rules=[struct(unreal.SovEncounterWaveReleaseRule,wave=1,
        condition=unreal.SovEncounterWaveCondition.COMMAND_SOURCE_DEFEATED,
        maximum_living_released_hostiles=2,command_link_participant_id=unreal.Name('E1.Drone2'),
        command_link_component_name=unreal.Name(lead.get_name()))])

    # Ground terminals replace only visible proxy machinery in the copied map.
    for side, xy, prefixes in (
            ('West',(5500,-10200),('Z04_Receiver_B_',)),
            ('East',(7400,-11100),('Z04_Receiver_A_',))):
        for label, a in ctx['existing'].items():
            if any(label.startswith(p) for p in prefixes):
                a.set_actor_hidden_in_game(True); a.set_actor_enable_collision(False)
        receiver = spawn(unreal.SovCampaignRelayReceiver,'E2_Receiver'+side,(xy[0],xy[1],66),-90)
        prop(receiver,receiver_id=unreal.Name('M12_E2_Receiver'+side),encounter_objective=e2_obj)
        receiver.visual.set_static_mesh(api.required('/Engine/BasicShapes/Cube'))
        receiver.visual.set_relative_scale3d(api.vector((.6,.9,1.3)))
        receiver.visual.set_material(0,api.required(api.BASE+'Materials/M_GB_blue'))
        result['receivers'].append(receiver)
    prop(e2_obj,required_receivers=result['receivers'])
    result['scanner_specs'] = [
        dict(scanner_id='Aurelion.Z03.Scanner17',position=(6200,-18100,200)),
        dict(scanner_id='Aurelion.Z03.Scanner32',position=(6200,-16600,200)),
    ]
    result['scanner_targets'] = [h['E2.Drone1'],h['E2.Drone2']]
    result['scanner_director'] = e2

    # A real powered sliding access door is the wave producer, not an invisible beat flag.
    door = spawn(unreal.SovWorldTransitActor,'E3_RescueAccess',(1100,8460,-470),90)
    prop(door,transit_id=unreal.Name('M12_E3_RescueApproach'),kind=unreal.SovWorldTransitKind.DOOR,
         destination_offset=api.vector((0,0,330)),travel_seconds=1.5,
         required_mission=unreal.Name(MISSION),irreversible_transition=False)
    # Keep 2.235cm from the original ramp rail and 1cm above the floor through the full native door sweep.
    door.moving_body.set_box_extent(api.vector((22,215,129)),False)
    door.visual.set_static_mesh(api.required('/Engine/BasicShapes/Cube'))
    door.visual.set_relative_scale3d(api.vector((.44,4.3,2.58)))
    door.visual.set_material(0,api.required(api.BASE+'Materials/M_GB_blue'))
    door.entry_bounds.set_box_extent(api.vector((210,230,180)),False)
    door.entry_bounds.set_relative_location(api.vector((-80,0,0)),False,False)
    door.interactable.set_editor_property('interactable_action_text',api.text('Action.RescueDoor','Open rescue approach'))
    prop(c3,wave_release_rules=[struct(unreal.SovEncounterWaveReleaseRule,wave=1,
        condition=unreal.SovEncounterWaveCondition.TRANSIT_DOOR_OPEN,
        maximum_living_released_hostiles=3,transit_door=door)])
    result['rescue_door'] = door
    # Future Weaver supports only its own reinforcement cohort; no hidden initial-wave armor.
    prop(h['E3.Weaver'].get_anchor_a(),linked_actors=[h['E3.Linkbound4']])
    prop(h['E3.Weaver'].get_anchor_b(),linked_actors=[h['E3.Linkbound5']])

    # Exact independent links, both receiving a real Selene sever receipt before handoff.
    weaver = h['E4.Weaver']; anchor_a,anchor_b = weaver.get_anchor_a(),weaver.get_anchor_b()
    prop(anchor_a,linked_actors=[h['E4.Elite'],h['E4.Linkbound1']])
    prop(anchor_b,linked_actors=[h['E4.Elite'],h['E4.Linkbound2']])
    bindings = [struct(unreal.SovAurelionCrucibleLink,participant_id=unreal.Name('E4.Weaver'),
        component_name=unreal.Name(link.get_name()),link_id=link.get_link_id()) for link in (anchor_a,anchor_b)]
    prop(phase_a,mission_id=unreal.Name(MISSION),elite_participant_id=unreal.Name('E4.Elite'),
         required_links=bindings,phase_b_objective=phase_b_obj,auto_request_handoff=False,auto_start_phase_b=True)
    prop(ca,wave_release_rules=[struct(unreal.SovEncounterWaveReleaseRule,wave=1,
        condition=unreal.SovEncounterWaveCondition.ACCEPTED_CRUCIBLE_LINK,maximum_living_released_hostiles=4)])
    result['crucible_links'] = [anchor_a,anchor_b]

    # Wall-route authoring uses the enemy builder's same tested physical specification.
    enemy_module_dir = Path(__file__).resolve().parent/'enemies/Scripts/Editor'
    if str(enemy_module_dir) not in sys.path: sys.path.insert(0,str(enemy_module_dir))
    from setup_aurelion_enemy_roles import wall_route_spec
    for group, origin in (('E3',(-1300,9300,-600)),('E4',(1500,21800,-1200))):
        half,radius = dimensions['WallRunner']; spec = wall_route_spec(half,radius)
        route = spawn(unreal.SovAurelionWallRoute,group+'_WallEntry',origin,90)
        prop(route,route_id=unreal.Name('Aurelion.'+group+'.WallEntry'),
             local_points=[api.vector(p) for p in spec['local_points']],
             wall_probe_direction=api.vector(spec['wall_probe_direction']),
             wall_probe_distance=spec['wall_probe_distance'],speed=spec['speed'],
             entry_tolerance=spec['entry_tolerance'],maximum_duration=spec['maximum_duration'])
        prop(h[group+'.WallRunner'].get_wall_traversal(),route=route)
        for shape in spec['geometry']:
            x,y,z = shape['center']
            box(group+'_'+shape['suffix'],(origin[0]-y,origin[1]+x,origin[2]+z),
                shape['extent'],'wall',90)
        result['wall_routes'].append(route)

    frost = spawn(unreal.TargetPoint,'E4_CleanFrost',(1000,21500,-1100),0)
    prop(frost,tags=list(frost.tags)+[unreal.Name('Aurelion.Crucible.CleanFrost')]
         if unreal.Name('Aurelion.Crucible.CleanFrost') not in frost.tags else list(frost.tags))
    box('E4_FrostFloorMark',(1000,21500,-1197),(95,95,3),'blue',0,False)
    thermal = h['E4.Elite'].get_thermal_fracture()
    prop(thermal,encounter_director=phase_b,frost_anchor=frost,
         frost_anchor_id=unreal.Name('Aurelion.Crucible.CleanFrost'))
    result['thermal'],result['frost_anchor'] = thermal,frost
    # All three are native requests; movement, frost and heat remain their owners' transactions.
    for suffix, op, position, words in (
        ('MovePartner',unreal.SovAurelionRequest.MOVE_FROST_PARTNER,(850,21350,-1130),'Ask Selene to take the frost mark'),
        ('FrostSetup',unreal.SovAurelionRequest.FROST_SETUP,(850,21700,-1130),'Ask Selene to arrest the exposed joint'),
        ('HeatConfirm',unreal.SovAurelionRequest.HEAT_CONFIRM,(550,21600,-1130),'Confirm the Cinder fracture')):
        a = spawn(unreal.SovAurelionRequestActor,'E4_'+suffix,position,-90)
        prop(a,request_id=unreal.Name('Aurelion.E4.'+suffix),mission_id=unreal.Name(MISSION),
             beat_id=unreal.Name('ThermalFracture'),operation=op,thermal=None,
             thermal_director=phase_b,thermal_participant_id=unreal.Name('E4.Elite'),
             action_text=api.text('Action.E4.'+suffix,words))
        a.visual.set_static_mesh(api.required('/Engine/BasicShapes/Cube'))
        a.visual.set_relative_scale3d(api.vector((.7,1.1,1.3)))
        a.visual.set_material(0,api.required(api.BASE+'Materials/M_GB_blue'))
        a.label.set_text(api.text('Action.E4.'+suffix))
        result['request_controls'].append(a)

    # One durable choice owner, two ordinary Narrative interaction options.
    for key, selection, position, words in (
        ('West',unreal.SovAurelionRescuePriority.WEST_STRETCHERS,(-120,15700,-839),'West stretchers\nRecovery cache'),
        ('East',unreal.SovAurelionRescuePriority.EAST_WALKERS,(120,15700,-839),'East walkers\nEarly flank shutter')):
        a = spawn(unreal.SovAurelionPriorityTerminal,'Priority'+key,position,-90)
        prop(a,priority=selection)
        box('Priority'+key+'_Visual',position,(25,35,60),'blue',-90,False)
        sign('Priority'+key+'_Label',words,(position[0],position[1],-720),-90,18)
        result['priority'][key] = a
    support = spawn(unreal.SovAurelionPrioritySupport,'PrioritySupport',(0,0,0),0)
    support.west_cache_barrier.set_world_location(api.vector((-3050,21985,-1060)),False,False)
    support.west_cache_barrier.set_box_extent(api.vector((120,20,140)),False)
    # The real elevated through-route, not the walking wounded's sole recess exit.
    support.east_flank_barrier.set_world_location(api.vector((2350,20700,-675)),False,False)
    support.east_flank_barrier.set_box_extent(api.vector((450,20,225)),False)
    # Cabinet surrounds enforce physical access; the native one-time cache is root's transaction owner.
    for suffix,xyz,extent in (
        ('West',(-3180,22110,-1060),(10,135,140)),
        ('East',(-2920,22110,-1060),(10,135,140)),
        ('Back',(-3050,22245,-1060),(130,10,140)),
        ('Roof',(-3050,22110,-910),(140,145,10))):
        box('RecoveryCacheCabinet_'+suffix,xyz,extent,'wall')
    result['support'] = support
    result['support_specs'] = dict(cache_position=(-3050,22110,-1140),
        west_gate_position=(-3050,21985,-1060),west_gate_extent=(120,20,140),
        east_gate_position=(2350,20700,-675),east_gate_extent=(450,20,225),
        manual_shutter_position=(2650,20550,-830))
    # Visible failed-entry requests, including overlap-disabled thermal phase B.
    # The existing objective/director owns restoration; these actors grant no proof.
    retry_floors = {
        'E1': (-6780,-17550,0), 'E2': (7180,-12850,0),
        'E3': (220,6650,-600), 'E4A': (220,18750,-1200),
        'E4B': (-480,19700,-1200),
    }
    result['retry_controls'] = {}
    for key, floor in retry_floors.items():
        existing_retry = present.get('Aurelion_'+key+'_RetryEncounter',[])
        if len(existing_retry)>1 or any(a.get_class()!=unreal.SovAurelionRequestActor.static_class()
                or unreal.Name(STAMP) not in a.tags for a in existing_retry):
            raise RuntimeError('Existing retry surface is ambiguous or unowned: '+key)
        position = checked_floor(key+'.RetrySurface',floor,45.,44.,existing_retry)
        control = spawn(unreal.SovAurelionRequestActor,key+'_RetryEncounter',position,-90)
        objective = result['objectives'][ENTRY[key][2]]
        director_owner = result['directors'][ENCOUNTERS[key]]
        if objective.encounter_director != director_owner:
            raise RuntimeError('Retry objective/director ownership differs: '+key)
        prop(control,request_id=unreal.Name('Aurelion.'+key+'.RetryEncounter'),mission_id=unreal.Name(MISSION),
             beat_id=unreal.Name(ENTRY[key][2]),operation=unreal.SovAurelionRequest.RETRY_ENCOUNTER,
             retry_objective=objective,retry_director=director_owner,
             story=None,handoff_anchor=None,co_action_anchor=None,thermal=None,
             thermal_director=None,thermal_participant_id=unreal.Name('None'),destination_mission=None,
             action_text=api.text('Action.RetryEncounter','Retry encounter'))
        control.visual.set_static_mesh(api.required('/Engine/BasicShapes/Cube'))
        control.visual.set_relative_scale3d(api.vector((.45,.65,.85)))
        control.visual.set_material(0,api.required(api.BASE+'Materials/M_GB_blue'))
        control.label.set_text(api.text('Action.RetryEncounter'))
        control.label.set_world_size(18.)
        result['retry_controls'][key] = control
        result['request_controls'].append(control)

    # Native cache owns one saved medical use; this script only supplies presentation and placement.
    old_cache = ctx['existing'].get('Z08_West_FieldCache_Placeholder')
    if old_cache:
        old_cache.set_actor_hidden_in_game(True); old_cache.set_actor_enable_collision(False)
    cache = spawn(unreal.SovAurelionMedicalCache,'WestMedicalCache',(-3050,22110,-1140),-90)
    prop(cache,cache_id=unreal.Name('Aurelion.West.MedicalAid'),support=support,health_fraction=.35)
    cache.visual.set_static_mesh(api.required('/Engine/BasicShapes/Cube'))
    cache.visual.set_relative_scale3d(api.vector((1,.8,.9)))
    cache.visual.set_material(0,api.required(api.BASE+'Materials/M_GB_blue'))
    support_view = spawn(unreal.SovAurelionSupportPresentation,'SupportBarrierView',(0,0,0),0)
    prop(support_view,support=support)
    for mesh in (support_view.west_barrier_visual,support_view.east_barrier_visual):
        mesh.set_static_mesh(api.required('/Engine/BasicShapes/Cube'))
        mesh.set_material(0,api.required(api.BASE+'Materials/M_GB_amber'))
    result['medical_cache'],result['support_presentation'] = cache,support_view
    result['handoff_destinations'] = {
        'HandoffToSelene': dict(request_position=(-6950,-8500,70),destination=(7000,-19400,100)),
        'HandoffToTarrikRescue': dict(request_position=(1150,-650,70),destination=(-550,700,100)),
        'HandoffToSeleneCage': dict(request_position=(-600,15300,-830),destination=(550,15000,-800)),
        'HandoffToTarrikCrucible': dict(phase_a=phase_a,phase_b=phase_b,objective=phase_b_obj,
            request_position=(0,19750,-1130),destination=(-250,19600,-1100)),
        'HandoffToSeleneAssent': dict(request_position=(-600,34800,-1710),destination=(550,34500,-1680)),
        'HandoffToTarrikAftermath': dict(request_position=(350,43000,70),destination=(-350,43050,100)),
    }
    # Warning acknowledgement follows an actually painted local cue; never bypass the native ranged gate.
    presentation = spawn(unreal.SovAurelionPresentationDirector,'EncounterWarnings',(0,0,0),0)
    prop(presentation,encounters=list(result['directors'].values()))
    result['presentation'] = presentation
    sign('E1_FormationHint','Pressure hall\nBreak the lead drone’s formation.',(-7000,-17500,250))
    sign('E2_ReceiversHint','Relay overlook\nDisable both receivers and clear the hostiles.',(7000,-12900,260))
    sign('E3_RescueHint','Shared breach\nOpen the blue rescue approach and protect the survivors.',(0,7000,-310))
    sign('E4_LinksHint','Quarantine crucible\nIsolate both Weaver links. Keep the elite alive.',(0,19000,-890))
    sign('E4_FrostHint','Thermal Fracture\nPosition Selene, arrest the joint, then confirm nearby.',(900,21300,-900),-90,21)
    result['requires_live_validation'] = [
        'All 24 NPCs initialize; exact native link instances exist before checkpoint entry.',
        'Native movement/attacks, four-then-two, four-then-three and first-link wave gates.',
        'Receivers use ordinary focus/hold, real door endpoint is traversable, both WallRunner sweeps succeed.',
        'Story cast remains available before entry and protected across retry and native E4 transfer.',
        'Request controls resolve the reconstructed elite after retry; no cached old component is admitted.',
        'Actual partner movement, live frost/heat receipt and conventional elite defeat.',
        'One-time 35% medical aid and exclusive support access; authenticated existing-drone scanners.',
    ]
    result['authored_actor_paths'] = [_path(a) for a in created]
    return result
