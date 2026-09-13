"""Author mission-owned Aurelion content in UE5.7. Native owners retain every gameplay receipt."""
import gc
import hashlib
import json
import math
import os
from pathlib import Path
import sys
import time
import traceback
import types
import unreal

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from story_content import SCENES, EVIDENCE

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
BASE = '/Game/Aurelion/'
SOURCE = BASE + 'Maps/L_Aurelion_Graybox_Velkorran'
TABLE = BASE + 'Data/ST_AurelionText'
STAMP = 'Sov.Aurelion.WorkPC.20260907'
MAPS = {12: BASE+'Maps/L_Aurelion_M12', 13: BASE+'Maps/L_Aurelion_M13'}
IDS = {12:'M12_FireAndFrost', 13:'M13_ContraryWitness'}
SOURCE_FILE = ROOT/'Content/Aurelion/Maps/L_Aurelion_Graybox_Velkorran.umap'
source_hash = hashlib.sha256(SOURCE_FILE.read_bytes()).hexdigest()
report = {'status':'started','source_map_sha256':source_hash,'created':[],'saved':[],'maps':{},'qualified_gameplay':False}
editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assets = unreal.AssetToolsHelpers.get_asset_tools()
strings = {}
table = None
source_objects = {}
source_memory = {}
source_files = {}
assembly = None

def required(path):
    obj = unreal.load_asset(path)
    if not obj: raise RuntimeError('Missing required content: '+path)
    return obj

def owned(path):
    if not path.startswith(BASE) or path == SOURCE:
        raise RuntimeError('Refusing to change unowned package: '+path)

def package(obj): return obj.get_path_name().split('.')[0]

def save(obj):
    path = package(obj); owned(path)
    assert_sources_unchanged()
    unreal.EditorAssetLibrary.set_metadata_tag(obj, STAMP, 'owned')
    if not unreal.EditorAssetLibrary.save_loaded_asset(obj, only_if_is_dirty=False):
        raise RuntimeError('Could not save '+path)
    report['saved'].append(path)

def new_asset(path, cls, factory=None):
    owned(path)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        obj = required(path)
        if unreal.EditorAssetLibrary.get_metadata_tag(obj, STAMP) != 'owned':
            raise RuntimeError('Existing content lacks this run ownership: '+path)
        if not isinstance(obj, cls): raise RuntimeError('Wrong existing class at '+path)
        return obj
    if factory is None:
        factory = unreal.DataAssetFactory(); factory.set_editor_property('data_asset_class', cls)
    folder, name = path.rsplit('/',1)
    obj = assets.create_asset(name,folder,cls,factory)
    if not obj: raise RuntimeError('Could not create '+path)
    unreal.EditorAssetLibrary.set_metadata_tag(obj,STAMP,'owned')
    report['created'].append(path)
    return obj

def bp(path, parent):
    f=unreal.BlueprintFactory(); f.set_editor_property('parent_class',parent)
    return new_asset(path,unreal.Blueprint,f)

def compile_bp(obj):
    unreal.BlueprintEditorLibrary.compile_blueprint(obj)
    if not obj.generated_class(): raise RuntimeError('Blueprint compilation failed '+package(obj))
    save(obj)

def prop(obj, **kwargs):
    for k,v in kwargs.items(): obj.set_editor_property(k,v)
    return obj

def vector(v): return unreal.Vector(*v)
def rot(yaw=90): return unreal.Rotator(pitch=0.0,yaw=float(yaw),roll=0.0)
def world(): return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
def tag(value):
    t=unreal.GameplayTag()
    if not t.import_text('(TagName="'+value+'")'): raise RuntimeError('Missing gameplay tag '+value)
    return t

def text(key, value=None):
    if value is not None:
        strings[key]=value
        if table is None: raise RuntimeError('Initialize the owned string table before text authoring')
        result=unreal.SovAurelionAuthoringLibrary.set_aurelion_strings(table,{key:value})
        if not result.get_editor_property('succeeded'): raise RuntimeError(result.report)
    return unreal.TextLibrary.text_from_string_table(unreal.Name(TABLE+'.ST_AurelionText'),key)

def spawn(cls,name,location,yaw=90,folder='Aurelion/Runtime'):
    a=actors.spawn_actor_from_class(cls,vector(location),rot(yaw))
    if not a: raise RuntimeError('Spawn failed '+name)
    a.set_actor_label('Aurelion_'+name); a.set_folder_path(folder)
    a.set_editor_property('tags',list(a.tags)+[unreal.Name(STAMP)])
    return a

def sign(name,label,location,yaw=-90,size=26):
    a=spawn(unreal.TextRenderActor,name,location,yaw)
    a.text_render.set_text(unreal.Text(label)); a.text_render.set_world_size(size)
    a.text_render.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
    return a

def mark(name, location, actor_tag=None):
    a=spawn(unreal.TargetPoint,name,location)
    if actor_tag: a.set_editor_property('tags',list(a.tags)+[unreal.Name(actor_tag)])
    return a

def decorate_terminal(a,action,key):
    prop(a,action_text=text(key,action))
    a.visual.set_static_mesh(required('/Engine/BasicShapes/Cube'))
    a.visual.set_relative_scale3d(unreal.Vector(.7,1.1,1.3))
    a.visual.set_material(0,required(BASE+'Materials/M_GB_blue'))
    a.label.set_text(text(key))
    # TextRender faces local +X; face the south approach in world space.
    a.label.set_world_rotation(rot(-90),False,False)
    label_forward = a.label.get_forward_vector()
    if not (abs(label_forward.x) < .001 and abs(label_forward.y + 1.) < .001 and abs(label_forward.z) < .001):
        raise RuntimeError('Owned terminal label does not face its south approach: '+a.get_actor_label())
    return a


def face_scene_request_label(chapter,beat,a,scene):
    # Presentation only. The fixed south-facing default still belongs to every
    # ordinary/retry/command terminal; only this exact PlayScene label rotates.
    w=world()
    if (chapter not in MAPS or editor.is_in_play_in_editor() or package(w)!=MAPS[chapter]
            or a.get_world()!=w or scene.get_world()!=w
            or a.operation!=unreal.SovAurelionRequest.PLAY_SCENE or a.story!=scene
            or str(a.mission_id)!=IDS[chapter] or str(a.beat_id)!=beat
            or STAMP not in [str(t) for t in a.tags]):
        raise RuntimeError('Scene label requires its exact stopped owned request: '+beat)
    p=scene.get_actor_location()
    approach=(1140.,-1270.,p.z) if beat=='MeetingAndCarrierRescue' else (
        (p.x-40.,p.y,p.z) if beat=='SeleneIndependentAssent' else (p.x+40.,p.y-70.,p.z))
    origin=a.label.get_world_location()
    dx,dy=approach[0]-origin.x,approach[1]-origin.y
    if not all(math.isfinite(float(v)) for v in (*approach,origin.x,origin.y,origin.z)) or math.hypot(dx,dy)<1.:
        raise RuntimeError('Scene label has no finite horizontal approach: '+beat)
    yaw=math.degrees(math.atan2(dy,dx))
    expected=(dx/math.hypot(dx,dy),dy/math.hypot(dx,dy))
    # Engine TextRender glyphs lie in local YZ with their visible normal on +X.
    a.label.set_world_rotation(rot(yaw),False,False)
    forward=a.label.get_forward_vector()
    dot=forward.x*expected[0]+forward.y*expected[1]
    if not math.isfinite(dot) or dot<.9999 or abs(forward.z)>.001:
        raise RuntimeError('Scene label rotation did not face its approach: '+beat)
    report.setdefault('scene_label_facing',[]).append(dict(
        chapter=chapter,beat=beat,actor=a.get_path_name(),label=a.label.get_path_name(),
        label_world_location=[origin.x,origin.y,origin.z],approach=list(approach),
        yaw=yaw,actual_forward=[forward.x,forward.y,forward.z],facing_dot=dot,
        scope='Only TextRender world yaw; size, text, position, body, interaction and gameplay unchanged',
        rendered_readability_verified=False))
    return a

def ordinary(beat,location,action):
    a=spawn(unreal.SovCampaignInteractionTerminal,beat,location)
    prop(a,terminal_id=unreal.Name('Aurelion_'+beat),mission_id=unreal.Name(IDS[12]),
         completion_beat=unreal.Name(beat),write_checkpoint=False)
    return decorate_terminal(a,action,'Action.'+beat)

def request(chapter,beat,operation,location,action,**targets):
    a=spawn(unreal.SovAurelionRequestActor,beat+'_'+str(operation),location)
    prop(a,request_id=unreal.Name('Aurelion_'+beat+'_'+str(operation).split('.')[-1]),mission_id=unreal.Name(IDS[chapter]),
         beat_id=unreal.Name(beat),operation=operation,**targets)
    return decorate_terminal(a,action,'Action.'+beat+'.'+str(operation).split('.')[-1])

def checkpoint(enum_value,name,location,extent=(220,180,200)):
    a=spawn(unreal.SovAurelionCheckpoint,name,location)
    prop(a,checkpoint=enum_value,capture_on_overlap=True)
    a.threshold.set_box_extent(vector(extent),False)
    return a

def handoff(chapter,beat,anchor_id,request_at,destination,action):
    # The request's physical prompt blocks visibility. Keep the native anchor's
    # standing eye-line above that body, with its destination still in world space.
    a=spawn(unreal.SovCampaignHandoffAnchor,anchor_id,(request_at[0],request_at[1],request_at[2]+90.))
    prop(a,mission_id=unreal.Name(IDS[chapter]),handoff_beat=unreal.Name(beat),anchor_id=unreal.Name(anchor_id))
    a.destination.set_world_location(vector(destination),False,False)
    a.destination.set_world_rotation(rot(),False,False)
    request(chapter,beat,unreal.SovAurelionRequest.HANDOFF,request_at,action,handoff_anchor=a)
    return a

def gate(chapter,beat,name,location,extent=(300,35,200),close_after=False,targets=()):
    a=spawn(unreal.SovAurelionJournalGate,name,location,0)
    prop(a,mission_id=unreal.Name(IDS[chapter]),beat_id=unreal.Name(beat),block_after_completion=close_after,
         bound_visual_actors=list(targets),use_gate_body=not bool(targets),closed_text=unreal.Text(''),open_text=unreal.Text(''))
    a.body.set_box_extent(vector(extent),False)
    a.visual.set_static_mesh(required('/Engine/BasicShapes/Cube'))
    a.visual.set_relative_scale3d(vector([v/50 for v in extent]))
    a.visual.set_material(0,required(BASE+'Materials/M_GB_amber'))
    return a

# Concrete scene stations on the imported walkable geometry, in centimetres.
SCENE_POSITIONS = {
    'MeetingAndCarrierRescue':(1100,-1200,90), 'FreeTrappedMarine':(650,8650,-510),
    'GroundLyric':(900,9500,-410), 'DestroyDominionResonator':(-900,14920,-810),
    'DestroyReformationCage':(900,14920,-810), 'ShareIsolatedThreatData':(0,15450,-810),
    'SurvivorsClearAndQuarantine':(0,23500,-1110), 'ContraryWitnessRecognized':(-400,28150,-1410),
    'TarrikIndependentAssent':(-250,34650,-1690), 'SeleneIndependentAssent':(250,34650,-1690),
    'MeridianContainment':(250,35000,-1690), 'FifthWitness':(250,35150,-1690),
    'GrammarPropagation':(250,35300,-1690), 'VoluntaryStay':(-400,42700,90),
    # Keep the recorder exchange beside the original conversation table.
    'CauldronRecorderReceived':(-500,43000,90), 'Record7283Received':(-400,43250,90),
    'ContainmentPact':(0,43500,90), 'SeparateDepartures':(-1100,47800,90),
}

def copy_struct(value):
    result=type(value)()
    if not result.import_text(value.export_text()): raise RuntimeError('Cannot copy native struct')
    return result

def retain_aurelion_npc(definition):
    """Keep mission-owned NPCs through Narrative's existing activity score gate."""
    owned(package(definition))
    library=unreal.GameplayTagLibrary
    original=copy_struct(definition.get_editor_property('default_owned_tags'))
    retention_tag=tag('Narrative.State.DontReturnToSpawn')
    if not library.is_gameplay_tag_valid(retention_tag):
        raise RuntimeError('Narrative retention tag is not registered')
    tags=list(library.break_gameplay_tag_container(original))
    if not library.has_tag(original,retention_tag,True): tags.append(retention_tag)
    retained=library.make_gameplay_tag_container_from_array(tags)
    definition.set_editor_property('default_owned_tags',retained)
    actual=definition.get_editor_property('default_owned_tags')
    if (not library.has_tag(actual,retention_tag,True) or not library.has_all_tags(actual,original,True)
            or library.get_num_gameplay_tags_in_container(actual) != len(tags)):
        raise RuntimeError('Narrative retention tag was not retained: '+package(definition))

def _companion_talk_template(blueprint, definition, hero):
    """Read the native Talk template on an explicitly dialogue-less owned proxy."""
    if (hero not in ('Tarrik','Selene') or package(blueprint)!=BASE+'Characters/BP_Aurelion'+hero+'Companion'
            or package(definition)!=BASE+'Characters/NPC_Aurelion'+hero+'Companion'
            or unreal.EditorAssetLibrary.get_metadata_tag(blueprint,STAMP)!='owned'
            or unreal.EditorAssetLibrary.get_metadata_tag(definition,STAMP)!='owned'
            or editor.is_in_play_in_editor()):
        raise RuntimeError('Companion Talk authoring requires the exact stopped owned assets')
    cdo=unreal.get_default_object(blueprint.generated_class())
    component=cdo.get_editor_property('npc_interactable_component')
    if (not isinstance(cdo,unreal.SovProtagonistCompanionCharacter)
            or not isinstance(component,unreal.NPCInteractable)
            or component.get_class()!=unreal.NPCInteractable.static_class()
            or component.get_owner()!=cdo or component.get_outer()!=cdo
            or package(component)!=package(blueprint)
            or definition.get_editor_property('dialogue') or definition.get_editor_property('tagged_dialogue_set')
            or component.get_editor_property('dialogue') or len(component.get_editor_property('interaction_slots'))):
        raise RuntimeError('Refusing to suppress an unowned or configured companion interaction: '+hero)
    fields={name:str(component.get_editor_property(name)) for name in
        ('interaction_distance','interaction_time','interaction_priority','max_view_angle_degrees',
         'interactable_name_text','interactable_action_text','dialogue','interaction_slots')}
    return component,fields


def disable_unconfigured_companion_talk(blueprint,definition,hero):
    component,before=_companion_talk_template(blueprint,definition,hero)
    parent=unreal.get_default_object(unreal.SovProtagonistCompanionCharacter.static_class())
    parent_component=parent.get_editor_property('npc_interactable_component')
    parent_before=bool(parent_component.get_editor_property('auto_activate'))
    previous=bool(component.get_editor_property('auto_activate'))
    # Mission commands use their own requests; this component has no dialogue or slots.
    component.set_editor_property('auto_activate',False)
    compile_bp(blueprint)
    current,after=_companion_talk_template(blueprint,definition,hero)
    if (current.get_editor_property('auto_activate') or before!=after
            or bool(parent_component.get_editor_property('auto_activate'))!=parent_before):
        raise RuntimeError('Companion Talk template did not survive compile/save without collateral changes: '+hero)
    report.setdefault('companion_talk_templates',{})[hero]=dict(
        blueprint=package(blueprint),definition=package(definition),component=current.get_path_name(),
        previous_auto_activate=previous,auto_activate=False,unrelated_fields_unchanged=True,
        fields=after,native_parent_auto_activate=parent_before,native_parent_unchanged=True,
        compiled_and_saved=True,fresh_runtime_focus_verified=False)


def validate_companion_talk_templates(heroes):
    for hero in ('Tarrik','Selene'):
        item=heroes[hero]
        component,fields=_companion_talk_template(item['companion_bp'],item['companion_npc'],hero)
        expected=report['companion_talk_templates'][hero]
        if component.get_editor_property('auto_activate') or fields!=expected['fields']:
            raise RuntimeError('Final companion Talk template readback differs: '+hero)
        expected['final_loaded_asset_readback_passed']=True


def make_profiles():
    heroes={}
    controller=unreal.load_class(None,'/NarrativePro/Pro/Core/AI/BP/BP_NarrativeNPCController.BP_NarrativeNPCController_C')
    if not controller: raise RuntimeError('Narrative controller content is unavailable')
    for hero in ('Tarrik','Selene'):
        player=required('/Game/Characters/Definitions/PD_'+hero)
        pawn=required('/Game/PlayerCharacters/BP_Sov'+hero).generated_class()
        companion_bp=bp(BASE+'Characters/BP_Aurelion'+hero+'Companion',unreal.SovProtagonistCompanionCharacter)
        cdo=unreal.get_default_object(companion_bp.generated_class())
        prop(cdo,ai_controller_class=controller,auto_possess_ai=unreal.AutoPossessAI.PLACED_IN_WORLD_OR_SPAWNED,
             tags=[unreal.Name('Aurelion_'+hero+'Actor')])
        compile_bp(companion_bp)
        configuration=new_asset(BASE+'Characters/AC_Aurelion'+hero+'Companion',unreal.AbilityConfiguration)
        player_configuration=required('/Game/Abilities/Configurations/AC_'+hero)
        default_attributes=player_configuration.get_editor_property('default_attributes')
        if not default_attributes: raise RuntimeError('Player attributes missing for '+hero)
        death=required('/NarrativePro/Pro/Core/Abilities/GameplayAbilities/GA_Death').generated_class()
        prop(configuration,default_attributes=default_attributes,startup_effects=[],default_abilities=[death])
        save(configuration)
        npc=new_asset(BASE+'Characters/NPC_Aurelion'+hero+'Companion',unreal.NPCDefinition)
        # Copy immutable presentation and faction values through Narrative's definition contract.
        for field in ('default_appearance','default_factions'):
            npc.set_editor_property(field,player.get_editor_property(field))
        prop(npc,character_id=unreal.Name(hero),npc_name=unreal.Text(hero),npc_class_path=companion_bp.generated_class(),
             allow_multiple_instances=True,default_currency=0,trading_currency=0,is_vendor=False,
             ability_configuration=configuration,
             activity_configuration=required('/NarrativePro/Pro/Core/AI/Configs/AC_Pacifist'),
             default_item_loadout=[],trading_item_loadout=[],dialogue=None,tagged_dialogue_set=None)
        retain_aurelion_npc(npc)
        from configure_aurelion_companion_equipment import configure as configure_companion_equipment, curated_combat_classes
        configure_companion_equipment(hero, npc)
        save(npc)
        disable_unconfigured_companion_talk(companion_bp,npc,hero)
        defense=required('/Game/Abilities/'+hero+'/GA_'+hero+('_Guard' if hero=='Tarrik' else '_Deflection')).generated_class()
        punch=required('/NarrativePro/Pro/Core/Abilities/GameplayAbilities/Attacks/Melee/GA_Melee_Punch_Unarmed').generated_class()
        # The native proxy copies only these classes already present in the outgoing actual kit.
        # The NPC baseline does not independently grant player attacks, inventory or ammunition.
        heroes[hero]={'player':player,'pawn':pawn,'companion_bp':companion_bp,'companion_npc':npc,
                      'companion_configuration':configuration,'curated':curated_combat_classes(hero)}
    return heroes

def make_evidence():
    result={}
    for identity,data in EVIDENCE.items():
        text('Evidence.'+identity+'.Summary',data['summary'])
        text('Evidence.'+identity+'.Text',data['text'])
    for identity,data in EVIDENCE.items():
        a=new_asset(BASE+'Evidence/DA_'+identity,unreal.SovEvidenceDefinition)
        prop(a,evidence_id=unreal.Name(identity),canonical_content_id=unreal.Name(identity),
             summary=text('Evidence.'+identity+'.Summary',data['summary']),full_text=text('Evidence.'+identity+'.Text',data['text']),
             original_custodian=unreal.Name(data['custodian']),source_custodians=[unreal.Name(data['custodian'])],
             relevant_missions=[unreal.Name(IDS[13])],critical_path=True)
        save(a)
        result[identity]=a
    return result

def _bind_mission_profiles(a,heroes):
    """Bind owned hero classes before map admission, including newly created DAs."""
    prop(a,pawn_class=heroes['Tarrik']['pawn'],player_definition=heroes['Tarrik']['player'])
    alternate=copy_struct(a.alternate_protagonists[0]); prop(alternate,pawn_class=heroes['Selene']['pawn'],player_definition=heroes['Selene']['player'])
    a.set_editor_property('alternate_protagonists',[alternate])
    profiles=[]
    for native_profile in a.protagonist_companions:
        p=copy_struct(native_profile); h=heroes[str(p.companion_id)]
        prop(p,companion_class=h['companion_bp'].generated_class(),companion_definition=h['companion_npc'],
             curated_companion_abilities=h['curated'])
        profiles.append(p)
    a.set_editor_property('protagonist_companions',profiles)


def make_missions(heroes,evidence,sequences):
    missions={}
    for chapter,cls in ((12,unreal.SovAurelionFireAndFrostMissionDefinition),(13,unreal.SovAurelionContraryWitnessMissionDefinition)):
        a=new_asset(BASE+'Data/DA_'+IDS[chapter],cls)
        _bind_mission_profiles(a,heroes)
        beats=[]; seqs={}
        for original in a.beats:
            b=copy_struct(original); key=IDS[chapter]+'.'+str(b.beat_id)
            prop(b,objective_text=text(key,str(b.objective_text)))
            if b.requires_cinematic_proof: seqs[b.cinematic_id]=sequences[str(b.beat_id)]
            evidence_id={'FifthWitness':'Aurelion_FifthWitness','CauldronRecorderReceived':'CauldronRecorder','Record7283Received':'Record7283'}.get(str(b.beat_id))
            if evidence_id: b.set_editor_property('critical_evidence',[evidence[evidence_id]])
            beats.append(b)
        prop(a,beats=beats,story_sequences=seqs)
        missions[chapter]=a
    return missions

def prepare_maps():
    """Copy the approved graybox; never edit its imported package."""
    for chapter,path in MAPS.items():
        owned(path)
        if not unreal.EditorAssetLibrary.does_asset_exist(path):
            if not editor.new_level_from_template(path,SOURCE): raise RuntimeError('Cannot copy '+path)
            unreal.EditorAssetLibrary.set_metadata_tag(world(),STAMP,'owned')
            if not editor.save_current_level(): raise RuntimeError('Cannot save copied map '+path)
            report['created'].append(path)
        # Existing worlds are opened through the level editor in begin_map. Loading
        # them here as standalone assets retains them across Blueprint compilation
        # and can dirty a world outside the editor's active-world lifecycle.


def seed_table():
    global table
    table=new_asset(TABLE,unreal.StringTable,unreal.StringTableFactory())
    text('Title.M12','Fire and Frost')
    text('Title.M13','Contrary Witness')
    save(table)


def begin_map(chapter):
    if not editor.load_level(MAPS[chapter]): raise RuntimeError('Cannot open '+MAPS[chapter])
    w=world()
    if package(w)!=MAPS[chapter]: raise RuntimeError('Wrong map after load')
    if unreal.EditorAssetLibrary.get_metadata_tag(w,STAMP) != 'owned':
        raise RuntimeError('Existing map is not owned by this authoring pass: '+MAPS[chapter])
    # Only actors stamped by this script are replaced on an iterative authoring run.
    for actor in list(actors.get_all_level_actors()):
        if unreal.Name(STAMP) in actor.tags:
            if not actor.get_path_name().startswith(w.get_path_name()+':'): raise RuntimeError('Actor escaped owned map')
            if not actors.destroy_actor(actor): raise RuntimeError('Cannot retire owned authoring actor')
    existing={a.get_actor_label():a for a in actors.get_all_level_actors()}
    for label,actor in existing.items():
        # The original map keeps its full designer annotations; the playable copy presents the mission.
        if isinstance(actor,unreal.TextRenderActor) or label.endswith('_StandIn'):
            actor.set_actor_hidden_in_game(True)
            actor.set_actor_enable_collision(False)
    starts=[a for a in existing.values() if isinstance(a,unreal.PlayerStart)]
    if len(starts)!=1: raise RuntimeError('Expected exactly one copied PlayerStart')
    start=starts[0]
    pos=(-7000,-20800,100) if chapter==12 else (-350,33000,-1700)
    start.set_actor_location(vector(pos),False,False)
    start.set_actor_rotation(rot(),False)
    start.set_editor_property('player_start_tag',unreal.Name('Aurelion_TarrikEntry' if chapter==12 else 'Aurelion_TarrikConvergence'))
    return existing


def solid(name,location,extent,material='floor',yaw=0):
    a=spawn(unreal.StaticMeshActor,name,location,yaw,folder='Aurelion/Runtime/Geometry')
    a.static_mesh_component.set_static_mesh(required('/Engine/BasicShapes/Cube'))
    a.static_mesh_component.set_material(0,required(BASE+'Materials/M_GB_'+material))
    a.set_actor_scale3d(vector([v/50 for v in extent]))
    a.static_mesh_component.set_collision_profile_name('BlockAll')
    return a


def author_lift(existing):
    for label,actor in existing.items():
        if label.startswith('F_Connectors_Lift_Proxy_'):
            actor.set_actor_hidden_in_game(True); actor.set_actor_enable_collision(False)
    # Real lift, with separate lower and upper landings in the approved connector footprint.
    solid('LiftLowerApproach',(0,38100,-1830),(300,500,30))
    solid('LiftUpperApproach',(0,40750,-30),(300,1550,30))
    for x in (-335,335):
        solid('LiftShaftSide_'+str(x),(x,38900,-850),(35,335,950),'wall')
        solid('LiftLowerGuard_'+str(x),(x,38100,-1710),(35,500,90),'wall')
        solid('LiftUpperGuard_'+str(x),(x,40750,90),(35,1550,90),'wall')
    lift=spawn(unreal.SovWorldTransitActor,'ExitLift',(0,38900,-1830),0)
    prop(lift,transit_id=unreal.Name('M13_AurelionExitLift'),kind=unreal.SovWorldTransitKind.LIFT,
         destination_offset=vector((0,0,1800)),travel_seconds=8.0,required_mission=unreal.Name(IDS[13]),
         required_beat=unreal.Name('GrammarPropagation'),irreversible_transition=False,
         require_mission_companion_aboard=True)
    lift.moving_body.set_box_extent(vector((290,290,30)),False)
    lift.visual.set_static_mesh(required('/Engine/BasicShapes/Cube'))
    lift.visual.set_relative_scale3d(vector((5.8,5.8,0.6)))
    lift.visual.set_material(0,required(BASE+'Materials/M_GB_blue'))
    lift.entry_bounds.set_box_extent(vector((290,290,220)),False)
    lift.entry_bounds.set_relative_location(vector((0,0,130)),False,False)
    lift.interactable.set_editor_property('interactable_action_text',text('Action.ExitLift','Ride the ascent lift'))
    lift.refresh_navigation_geometry()
    sign('LiftBoarding','ASCENT LIFT\nLet your companion board, then hold Interact.',(0,38620,-1530),-90,22)
    return lift


def author_route(chapter,existing,scene_result,encounter_result,missions=None):
    S=unreal.SovAurelionCheckpointBoundary
    if chapter==12:
        ordinary('TarrikArrival',(-6680,-20000,70),'Confirm the route with the wounded guide')
        ordinary('SecureTarrikRoute',(-6650,-9000,70),'Secure the approach')
        ordinary('SeleneArrival',(7250,-13200,70),'Survey the relay overlook')
        handoff(12,'HandoffToSelene','M12_SeleneEntry',(-6950,-8500,70),(7000,-19400,100),'Continue as Selene')
        handoff(12,'HandoffToTarrikRescue','M12_TarrikSharedBreach',(1150,-650,70),(-550,1000,90),'Lead the shared breach as Tarrik')
        handoff(12,'HandoffToSeleneCage','M12_SeleneCage',(-600,15300,-830),(550,15000,-800),'Continue the cage work as Selene')
        crucible_handoff=handoff(12,'HandoffToTarrikCrucible','M12_TarrikCrucible',(0,19750,-1130),(-250,19600,-1100),'Take Tarrik’s side of the crucible')
        prop(encounter_result['directors']['M12_E4_QuarantineCrucibleA'],handoff_anchor=crucible_handoff)
        for enum_id,name,pos,extent in [
            (S.CONTEXT_CP0,'CP0',(-7000,-20800,100),(400,350,200)),
            (S.SELENE_ENTRY_CP2,'CP2',(7000,-19400,100),(450,350,200)),
            (S.MEETING_CP3,'CP3',(0,-2000,100),(1450,400,200)),
            (S.QUARANTINE_CP6,'CP6',(0,25750,-1400),(650,400,200)),
        ]: checkpoint(enum_id,name,pos,extent)
        for beat,name,pos,ext in [
            ('TarrikArrival','TarrikArrivalGate',(-7000,-18800,180),(600,35,180)),
            ('PressureHall','PressureHallExit',(-7000,-11700,180),(900,35,180)),
            ('RelayOverlook','RelayOverlookExit',(7000,-8850,180),(700,35,180)),
            ('MeetingAndCarrierRescue','SharedRouteAccess',(0,3350,180),(700,35,180)),
            ('GroundLyric','JunctionExit',(0,11800,-420),(750,35,180)),
            ('LocalPriorityCommitted','CrucibleEntry',(0,17900,-940),(600,35,180)),
            ('ThermalFracture','CrucibleEvacuationAccess',(0,23100,-1020),(950,35,180)),
            ('SurvivorsClearAndQuarantine','RecognitionAccess',(0,24600,-1170),(650,35,180)),
        ]: gate(12,beat,name,pos,ext)
        gate(12,'SurvivorsClearAndQuarantine','QuarantineSealed',(0,23150,-1020),(950,35,180),True)
        gate(12,'ContraryWitnessRecognized','MissionTransitionBoundary',(0,31000,-1520),(650,35,220))
        if missions:
            request(12,'ContraryWitnessRecognized',unreal.SovAurelionRequest.TRAVEL_TO_MISSION,
                    (-300,29600,-1430),'Continue to the terminal core',destination_mission=missions[13])
    else:
        spawn(unreal.SovAurelionDeparturePresentation,'SeparateDeparturePresentation',(0,0,0),0)
        handoff(13,'HandoffToSeleneAssent','M13_SeleneAssent',(-600,34800,-1710),(550,34500,-1680),'Give Selene her independent answer')
        handoff(13,'HandoffToTarrikAftermath','M13_TarrikAftermath',(350,43000,70),(-350,43050,100),'Continue the exchange as Tarrik')
        anchor=spawn(unreal.SovCoActionAnchor,'ContraryPosition',(-250,34250,-1710))
        prop(anchor,anchor_id=unreal.Name('M13_SeleneContraryPosition'),mission_id=unreal.Name(IDS[13]),
             completion_beat=unreal.Name('ContraryPosition'),required_companion_id=unreal.Name('Selene'),request_range=500.0)
        anchor.companion_mark.set_world_location(vector((350,34600,-1800)),False,False)
        request(13,'ContraryPosition',unreal.SovAurelionRequest.CO_ACTION,(-250,34250,-1710),
                'Ask Selene to take the contrary position',co_action_anchor=anchor)
        checkpoint(S.CORE_CP7,'CP7',(0,37200,-1700),(800,400,200))
        # The checkpoint lies on the post-conversation return through its actual scene station.
        checkpoint(S.CONVERSATION_CP8,'CP8',(-350,42700,100),(450,350,200))
        checkpoint(S.DEPARTURE_CP9,'CP9',(-1100,48100,100),(1400,600,200))
        author_lift(existing)
        gate(13,'GrammarPropagation','CoreExit',(0,37600,-1620),(350,35,180))
        gate(13,'ContainmentPact','DepartureAccess',(0,44500,180),(650,35,180))
    for beat,scene in scene_result['scenes'].items():
        p=SCENE_POSITIONS[beat]
        # Actor bodies stand beside the character marks, leaving their exit capsules unobstructed.
        # The curved Meeting platform has no floor beyond its outer guard.
        # Keep its request inside the platform; scene marks and exits stay authored.
        at=(910,-1300,65) if beat=='MeetingAndCarrierRescue' else (p[0]+180,p[1]-170,p[2]-25)
        if beat=='ShareIsolatedThreatData':
            # Clear the original auxiliary desk's north face by 50cm.
            at=(180,15350,-835)
        # Keep each complete box on the circular dais and clear of the original table.
        m13_request_positions={
            'TarrikIndependentAssent':(-70,34580,-1715),
            'SeleneIndependentAssent':(330,34700,-1715),
            'MeridianContainment':(350,34830,-1715),
            'Record7283Received':(-600,43120,65),
        }
        if beat in m13_request_positions:
            at=m13_request_positions[beat]
        if beat=='SurvivorsClearAndQuarantine':
            from align_aurelion_scene_request_floor import sample_scene_request_floor
            at=sample_scene_request_floor(world(),beat,at,report.setdefault('scene_request_floor_alignment',[]))
        scene_request=request(chapter,beat,unreal.SovAurelionRequest.PLAY_SCENE,at,
                'Continue: '+str(next(row[3][0][0] for row in SCENES if row[0]==beat)),story=scene)
        face_scene_request_label(chapter,beat,scene_request,scene)
    return True


def _encode(value):
    if value is None or isinstance(value,(bool,int,float,str)): return value
    if isinstance(value,unreal.Object): return value.get_path_name()
    if isinstance(value,dict): return {str(k):_encode(v) for k,v in value.items()}
    if isinstance(value,(list,tuple,unreal.Array)): return [_encode(v) for v in value]
    if hasattr(value,'export_text'): return value.export_text()
    return str(value)


def _write_report():
    OUT.mkdir(parents=True,exist_ok=True)
    report['source_sha256_after']=hashlib.sha256(SOURCE_FILE.read_bytes()).hexdigest()
    report['source_unchanged']=report['source_sha256_after']==source_hash
    tmp=OUT/'full-route-authoring.tmp'
    tmp.write_text(json.dumps(_encode(report),indent=2),encoding='utf-8')
    tmp.replace(OUT/'full-route-authoring.json')


def capture_source_guards():
    """Materialize original class defaults before a read-only persistent-property snapshot."""
    paths=['/Game/Framework/BP_SovGameMode_Tarrik','/Game/Framework/BP_SovPlayerController',
           '/Game/Framework/BP_SovPlayerState']
    for hero in ('Tarrik','Selene'):
        paths.extend(['/Game/Characters/Definitions/PD_'+hero,
                      '/Game/PlayerCharacters/BP_Sov'+hero,
                      '/Game/Abilities/Configurations/AC_'+hero])
    for path in paths:
        obj=required(path); source_objects[path]=obj
        if isinstance(obj,unreal.Blueprint):
            if not obj.generated_class(): raise RuntimeError('Source has no compiled class: '+path)
            unreal.get_default_object(obj.generated_class())
        disk=ROOT/'Content'/Path(path[len('/Game/'):]+'.uasset')
        if not disk.is_file(): raise RuntimeError('Source package has no disk evidence: '+path)
        source_files[str(disk)]=hashlib.sha256(disk.read_bytes()).hexdigest()
    # The compiler's metadata is not used as gameplay proof. GC settles unused editor objects first.
    gc.collect(); unreal.collect_garbage()
    for path,obj in source_objects.items():
        source_memory[path]=_source_signature(obj)
    report['source_package_sha256']=dict(source_files)
    report['source_memory_before']=dict(source_memory)


def _source_signature(obj):
    if isinstance(obj,unreal.Blueprint):
        value=unreal.SovBlueprintAuthoringLibrary.fingerprint_blueprint(obj)
        if value=='InvalidBlueprint': raise RuntimeError('Invalid Blueprint preservation target')
        return {'blueprint':value}
    if isinstance(obj,unreal.AbilityConfiguration):
        fields=('default_attributes','startup_effects','default_abilities')
    elif isinstance(obj,unreal.PlayerDefinition):
        fields=('default_appearance','default_factions','ability_configuration','default_item_loadout')
    else: raise RuntimeError('Unknown source preservation schema: '+obj.get_path_name())
    return {'class':obj.get_class().get_path_name(),'fields':{field:_encode(obj.get_editor_property(field)) for field in fields}}


def assert_sources_unchanged():
    if hashlib.sha256(SOURCE_FILE.read_bytes()).hexdigest()!=source_hash:
        raise RuntimeError('The original imported graybox changed; saves stopped.')
    for path,digest in source_files.items():
        if hashlib.sha256(Path(path).read_bytes()).hexdigest()!=digest:
            raise RuntimeError('An original content package changed on disk: '+path)
    for path,before in source_memory.items():
        after=_source_signature(source_objects[path])
        if after!=before: raise RuntimeError('An original content object changed in memory: '+path)


def _stopped():
    if editor.is_in_play_in_editor(): raise RuntimeError('Stop PIE before full map authoring')


def _floor_point(position,half_height=94.0):
    ignored=[a for a in actors.get_all_level_actors() if isinstance(a,(unreal.Pawn,unreal.PlayerStart))]
    result=unreal.SystemLibrary.line_trace_single(world(),vector((position[0],position[1],position[2]+150)),
        vector((position[0],position[1],position[2]-1000)),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
        False,ignored,unreal.DrawDebugTrace.NONE,True)
    hit=result if isinstance(result,unreal.HitResult) else next((v for v in result if isinstance(v,unreal.HitResult)),None) if isinstance(result,tuple) else None
    values=hit.to_tuple() if hit else None
    if not values or not values[0] or values[7].z<.7:
        raise RuntimeError('Mandatory entry/companion mark has no walkable floor: '+str(position))
    point=values[5]
    return (float(position[0]),float(position[1]),float(point.z)+half_height+2)


def author_entry_anchors(chapter,heroes):
    """Unique recovery/initial-entry tags; native shared handoff still spawns at the outgoing pawn."""
    positions={12:{'Tarrik':(-150,900,100),'Selene':(150,1150,100)},
               13:{'Tarrik':(-650,33100,-1690),'Selene':(350,33100,-1690)}}[chapter]
    made={}
    for hero,requested in positions.items():
        anchor_tag='Aurelion_'+hero+'Companion'
        existing=[a for a in actors.get_all_level_actors() if unreal.Name(anchor_tag) in a.tags]
        if existing: raise RuntimeError('Recovery anchor tag already exists outside this authoring pass: '+anchor_tag)
        cdo=unreal.get_default_object(heroes[hero]['companion_bp'].generated_class())
        half=cdo.get_editor_property('capsule_component').get_scaled_capsule_half_height()
        a=mark(hero+'CompanionEntry',_floor_point(requested,half),anchor_tag)
        made[hero]=a
    starts=[a for a in actors.get_all_level_actors() if isinstance(a,unreal.PlayerStart)]
    if len(starts)!=1: raise RuntimeError('Full wrapper must retain one PlayerStart')
    start=starts[0]
    half=unreal.get_default_object(heroes['Tarrik']['pawn']).get_editor_property('capsule_component').get_scaled_capsule_half_height()
    p=start.get_actor_location()
    start.set_actor_location(vector(_floor_point((p.x,p.y,p.z),half)),False,False)
    for hero,a in made.items():
        matches=[candidate for candidate in actors.get_all_level_actors() if unreal.Name('Aurelion_'+hero+'Companion') in candidate.tags]
        if matches!=[a]: raise RuntimeError('Companion entry tag is not unique: '+hero)
    return made


def make_mode(chapter,mission):
    """Inherit project framework and opt only Aurelion into native recovery UI."""
    source=required('/Game/Framework/BP_SovGameMode_Tarrik')
    if not isinstance(unreal.get_default_object(source.generated_class()),unreal.SovCampaignGameMode):
        raise RuntimeError('Project framework is not the native campaign GameMode')
    source_cdo=unreal.get_default_object(source.generated_class())
    source_pc=required('/Game/Framework/BP_SovPlayerController')
    if source_cdo.get_editor_property('player_controller_class')!=source_pc.generated_class():
        raise RuntimeError('Project GameMode controller dependency changed')
    pc=bp(BASE+'Framework/BP_AurelionPlayerController',source_pc.generated_class())
    pc_cdo=unreal.get_default_object(pc.generated_class())
    failure_menu=unreal.get_default_object(source_pc.generated_class()).get_editor_property('DeathMenuClass')
    if not failure_menu or failure_menu.get_path_name()!='/Game/UI/Narrative/Menus/Fail/W_NarrativeMenu_Failed.W_NarrativeMenu_Failed_C':
        raise RuntimeError('Requires the existing actionable project failure menu')
    source_pause=unreal.get_default_object(source_pc.generated_class()).get_editor_property('PauseMenuClass')
    if not source_pause or source_pause.get_path_name()!='/Game/UI/Narrative/Menus/Pause/W_NarrativeMenu_Pause.W_NarrativeMenu_Pause_C':
        raise RuntimeError('Project pause-menu dependency changed')
    pause_class=unreal.SovAurelionPauseMenu.static_class()
    prop(pc_cdo,fatal_recovery_failure_menu_class=failure_menu,PauseMenuClass=pause_class)
    import setup_aurelion_weapon_hud
    setup_aurelion_weapon_hud.build_weapon_hud(types.SimpleNamespace(**globals()),pc)
    pc_cdo=unreal.get_default_object(pc.generated_class())
    if not unreal.SovAurelionEnemyAuthoringLibrary.compile_owned_blueprint(pc,source_pc.generated_class()):
        raise RuntimeError('Aurelion controller compilation failed')
    save(pc)
    pc_cdo=unreal.get_default_object(pc.generated_class())
    if (pc_cdo.get_editor_property('fatal_recovery_failure_menu_class')!=failure_menu
            or pc_cdo.get_editor_property('DeathMenuClass')!=failure_menu):
        raise RuntimeError('Compiled Aurelion controller lost its exact failure-menu integration')
    if (pc_cdo.get_editor_property('PauseMenuClass')!=pause_class
            or unreal.get_default_object(source_pc.generated_class()).get_editor_property('PauseMenuClass')!=source_pause):
        raise RuntimeError('Owned campaign pause-menu assignment or source guard failed')
    report['campaign_pause_menu']={'controller':package(pc),'class':pause_class.get_path_name(),
        'native_checkpoint_kind':'Checkpoint','slot_index':0,'stock_save_load_selector':False,
        'source_pause_menu':source_pause.get_path_name(),'source_controller_unchanged':True}
    report['fatal_recovery_presentation']={'controller':package(pc),'parent':package(source_pc),
        'failure_menu':failure_menu.get_path_name(),'source_controller_unchanged':True}
    mode=bp(BASE+'Framework/BP_AurelionGameMode_M'+str(chapter),source.generated_class())
    cdo=unreal.get_default_object(mode.generated_class())
    prop(cdo,initial_mission=mission,use_seamless_travel=False,player_controller_class=pc.generated_class())
    if not unreal.SovAurelionEnemyAuthoringLibrary.compile_owned_blueprint(mode,source.generated_class()):
        raise RuntimeError('Aurelion GameMode compilation failed')
    save(mode)
    cdo=unreal.get_default_object(mode.generated_class())
    if cdo.get_editor_property('initial_mission')!=mission:
        raise RuntimeError('Compiled GameMode lost its exact mission')
    if cdo.get_editor_property('player_controller_class')!=pc.generated_class():
        raise RuntimeError('Compiled GameMode lost its mission-owned controller')
    for field in ('player_state_class','game_state_class','hud_class'):
        if cdo.get_editor_property(field)!=source_cdo.get_editor_property(field):
            raise RuntimeError('Wrapper changed authored framework dependency: '+field)
    settings=world().get_world_settings()
    settings.set_editor_property('default_game_mode',mode.generated_class())
    return mode


NAV_CHECKS={
    12:[('Tarrik approach',(-7000,-20800,100),(-6750,-20200,100)),
        ('Pressure hall',(-7000,-17200,100),(-7000,-16000,100)),
        ('Selene scan lane',(7000,-19400,100),(7000,-18300,100)),
        ('Relay west receiver',(7000,-12500,100),(5500,-10400,100)),
        ('Selene carrier seam',(7000,-4900,100),(7000,-4450,100)),
        ('Selene connector length',(7000,-8500,100),(7000,-2600,100)),
        ('Selene east dogleg',(7000,-6000,100),(5000,0,100)),
        ('Shared breach',(0,6800,-500),(0,7600,-500)),
        ('Rescue door approach',(1100,8160,-500),(1100,8330,-500)),
        ('Crucible entry',(0,18900,-1100),(0,19600,-1100)),
        ('Crucible frost approach',(500,21000,-1100),(1000,21400,-1100))],
    13:[('Convergence entry',(-350,33100,-1700),(-350,34100,-1700)),
        ('Lower lift landing',(0,37900,-1700),(0,38600,-1700)),
        ('Lift boarding',(0,38400,-1710),(120,39000,-1710)),
        ('Lift lower disembark',(120,39000,-1710),(0,38400,-1710)),
        ('Upper lift landing',(0,40100,100),(0,41300,100)),
        ('Aftermath approach',(0,41900,100),(-350,42600,100)),
        ('Departure route',(-1100,47000,100),(-1350,47800,100))],
}


def _begin_navigation():
    w=world()
    # Give the copied world its durable, owned navigation profile for the actual capsules.
    dimensions=[]
    def record_capsule(kind,name,cls):
        cdo=unreal.get_default_object(cls)
        capsule=cdo.get_editor_property('capsule_component')
        radius=float(capsule.get_scaled_capsule_radius())
        half_height=float(capsule.get_scaled_capsule_half_height())
        if not math.isfinite(radius) or not math.isfinite(half_height) or radius<=0 or half_height<radius:
            raise RuntimeError('Invalid authored navigation capsule: '+kind+'/'+name)
        dimensions.append({'kind':kind,'name':name,'class':cls.get_path_name(),
            'capsule':capsule.get_path_name(),'radius':radius,'half_height':half_height,'height':2*half_height})
    for name,row in sorted(assembly['heroes'].items()):
        record_capsule('hero',name,row['pawn'])
    expected_roles={'SecurityDrone','ContaminatedDrone','Enforcer','Linkbound','WallRunner','Weaver','Elite'}
    if set(assembly['enemies'])!=expected_roles:
        raise RuntimeError('Navigation requires the exact seven authored enemy roles')
    for role,row in sorted(assembly['enemies'].items()):
        class_path=row['class']
        if not class_path.startswith(BASE+'Enemies/BP_Aurelion'+role+'.'):
            raise RuntimeError('Navigation enemy class is outside its owned role: '+str(class_path))
        cls=unreal.load_class(None,class_path)
        if not cls or not isinstance(unreal.get_default_object(cls),unreal.SovNPCCharacterBase):
            raise RuntimeError('Navigation enemy lacks its actual native class: '+role)
        record_capsule('enemy',role,cls)
    maximum_radius=max(row['radius'] for row in dimensions)
    maximum_height=max(row['height'] for row in dimensions)
    configuration=unreal.SovAurelionNavigationProfileLibrary.configure_navigation(w,float(maximum_radius),float(maximum_height))
    if not configuration.configured:
        raise RuntimeError('Native Aurelion navigation configuration failed: '+str(configuration.error))
    # The saved world profile selects matching native data before registration.
    built=unreal.SovAurelionNavigationLibrary.build_navigation(w,vector((0,13000,-500)),vector((14000,38000,3000)))
    volume=built if isinstance(built,unreal.NavMeshBoundsVolume) else next((v for v in built if isinstance(v,unreal.NavMeshBoundsVolume)),None) if isinstance(built,tuple) else None
    if volume is None: raise RuntimeError('Native navigation authoring failed: '+str(built))
    assembly['nav_started']=time.monotonic(); assembly['nav_idle_ticks']=0
    assembly['nav_volume']=volume
    assembly['phase']='navigation'
    report['phase']='navigation M'+str(assembly['chapter'])
    report['maps'][str(assembly['chapter'])]['navigation']={'requested':True,'volume':volume.get_path_name(),
        'agent_radius':maximum_radius,'agent_height':maximum_height,'capsule_dimensions':dimensions,
        'runtime_generation':'Dynamic','paths':[],
        'door_and_lift_runtime_traversal_verified':False}
    _write_report()


def _project_nav(position):
    value=unreal.NavigationSystemV1.project_point_to_navigation(world(),vector(position),None,None,vector((160,160,350)))
    projected=value if isinstance(value,unreal.Vector) else next((v for v in value if isinstance(v,unreal.Vector)),None) if isinstance(value,tuple) else None
    if projected is None or (isinstance(value,tuple) and any(v is False for v in value)):
        raise RuntimeError('Navigation projection failed at '+str(position))
    if abs(projected.x-position[0])>160 or abs(projected.y-position[1])>160 or abs(projected.z-position[2])>350:
        raise RuntimeError('Navigation projection escaped its bounded query')
    return projected


def _validate_carrier_corridor_capsules(evidence, name, path):
    # Read-only physical confirmation after final authoring/nav generation. Mobile
    # Narrative characters are excluded; no carrier, wall, floor or other scenery is.
    ignored=[]
    for character in unreal.GameplayStatics.get_all_actors_of_class(world(),unreal.NarrativeCharacter):
        ignored.append(character)
        ignored.extend(character.get_attached_actors())
        visual=character.get_character_visual()
        if visual is not None and visual not in ignored: ignored.append(visual)
    points=list(path.path_points)
    if len(points)<2: raise RuntimeError('Carrier corridor requires an actual multi-point path: '+name)
    output=evidence.setdefault('carrier_corridor_capsule_sweeps',[])
    for hero in (row for row in evidence['capsule_dimensions'] if row['kind']=='hero'):
        radius,half=float(hero['radius']),float(hero['half_height'])
        for index,(a,b) in enumerate(zip(points,points[1:])):
            # Recast points lie on the nav surface; use the real capsule half-height
            # plus 2cm clearance, rather than sweeping its center through the floor.
            start=unreal.Vector(a.x,a.y,a.z+half+2.)
            end=unreal.Vector(b.x,b.y,b.z+half+2.)
            result=unreal.SystemLibrary.capsule_trace_single_by_profile(world(),start,end,radius,half,
                unreal.Name('Pawn'),False,ignored,unreal.DrawDebugTrace.NONE,True)
            hit=None
            if result is not None:
                candidates=result if isinstance(result,tuple) else (result,)
                hits=[value for value in candidates if isinstance(value,unreal.HitResult)]
                if len(hits)!=1: raise RuntimeError('Unexpected native capsule sweep output: '+str(result))
                hit=hits[0]
            row={'path':name,'hero':hero['name'],'capsule_class':hero['class'],'segment':index,
                 'start':[start.x,start.y,start.z],'end':[end.x,end.y,end.z],
                 'radius':radius,'half_height':half,'collision_profile':'Pawn',
                 'ignored':'mobile Narrative characters and their attachments only',
                 'blocking':bool(hit.to_tuple()[0]) if hit else False,'hit':hit.export_text() if hit else None}
            output.append(row)
            if row['blocking']:
                raise RuntimeError('Authored carrier corridor blocks actual '+hero['name']+' capsule: '+name+' '+str(row['hit']))


def _validate_navigation():
    chapter=assembly['chapter']
    evidence=report['maps'][str(chapter)]['navigation']
    inspection=unreal.SovAurelionNavigationProfileLibrary.inspect_navigation(world())
    evidence['native_profile']=inspection.export_text()
    if not inspection.configured or not inspection.registered or not inspection.dynamic:
        raise RuntimeError('Saved navigation profile is not registered correctly: '+str(inspection.error))
    for name,start,end in NAV_CHECKS[chapter]:
        a,b=_project_nav(start),_project_nav(end)
        path=unreal.SovAurelionNavigationLibrary.find_path_to_location_synchronously(world(),a,b,None,None)
        if path is None or not path.is_valid() or path.is_partial():
            raise RuntimeError('Required local navigation path is missing/partial: '+name)
        if name in ('Lift boarding','Lift lower disembark'):
            points=list(path.path_points)
            if len(points)<2 or any(math.hypot(p.x-target[0],p.y-target[1])>35.0
                                   for p,target in ((points[0],start),(points[-1],end))):
                raise RuntimeError('Lift dock path clamps outside its actual requested endpoints: '+name)
        evidence['paths'].append({'name':name,'start':[a.x,a.y,a.z],'end':[b.x,b.y,b.z],
            'complete':True,'length_cm':path.get_path_length(),'point_count':len(path.path_points)})
        if name in ('Selene carrier seam','Selene connector length','Selene east dogleg'):
            _validate_carrier_corridor_capsules(evidence,name,path)
    from validate_aurelion_scene_requests import validate_scene_request_surfaces
    validate_scene_request_surfaces(assembly['context'],report['maps'][str(chapter)]['scene_report'])
    if chapter == 12:
        from validate_aurelion_transit_clearance import validate_rescue_door_clearance
        validate_rescue_door_clearance(world(),report['maps']['12'].setdefault('rescue_door_clearance',{}))
    from validate_aurelion_handoff_destinations import validate_handoff_destinations
    validate_handoff_destinations(assembly['context'],report['maps'][str(chapter)].setdefault('handoff_destinations',{}))
    evidence['built']=True
    evidence['elapsed_seconds']=round(time.monotonic()-assembly['nav_started'],3)


def _mission_shells(heroes):
    missions={chapter:new_asset(BASE+'Data/DA_'+IDS[chapter],cls) for chapter,cls in
        ((12,unreal.SovAurelionFireAndFrostMissionDefinition),(13,unreal.SovAurelionContraryWitnessMissionDefinition))}
    for mission in missions.values():
        _bind_mission_profiles(mission,heroes)
    return missions


def _assemble_current_map():
    _stopped(); assert_sources_unchanged()
    chapter=assembly['chapter']; existing=begin_map(chapter)
    # UE's Python soft-world setter takes a UWorld wrapper. Bind the current editor
    # world here so final mission authoring never loads another map as an asset.
    assembly['missions'][chapter].set_editor_property('map',world())
    anchors=author_entry_anchors(chapter,assembly['heroes'])
    # ExecutePythonScript owns a script-global dictionary distinct from Python's __main__ module.
    context=dict(chapter=chapter,world=world(),api=types.SimpleNamespace(**globals()),heroes=assembly['heroes'],
                 existing=existing,enemy_definitions=assembly['enemies'],positions=SCENE_POSITIONS,
                 scanner_controller=assembly['scanner_controller'])
    from cinematic_content import build_cinematics, validate_static_scene_exits
    from encounter_content import build_encounters
    from setup_aurelion_scanners import build_scanners
    scene_result=build_cinematics(context)
    context['cast']=scene_result['cast']
    encounter_result=build_encounters(context)
    scanners=build_scanners(context,encounter_result,OUT)
    author_route(chapter,existing,scene_result,encounter_result,assembly['missions'])
    mode=make_mode(chapter,assembly['missions'][chapter])
    assembly['sequences'].update(scene_result['sequences'])
    # Retain the current world until its navigation/save transaction completes; clear before loading the next.
    assembly['context']=context
    report['maps'][str(chapter)]={'map':MAPS[chapter],'scene_report':_encode(scene_result['report']),
        'encounters':_encode(encounter_result),'scanners':_encode(scanners),
        'companion_anchors':_encode(anchors),'game_mode':mode.get_path_name(),'saved':False}
    from setup_aurelion_npc_identities import assign as assign_npc_identities
    identity_report={}
    report['maps'][str(chapter)]['stable_npc_identities']=identity_report
    assign_npc_identities(context,encounter_result,identity_report)
    try:
        validate_static_scene_exits(context,scene_result)
    finally:
        report['maps'][str(chapter)]['scene_report']=_encode(scene_result['report'])
    _begin_navigation()


def _validate_native_mission(mission):
    response=mission.validate_definition()
    if response is None or response is False or (isinstance(response,str) and response):
        raise RuntimeError('Native mission validation failed: '+str(response))
    if isinstance(response,tuple) and (any(v is False for v in response) or any(isinstance(v,str) and v for v in response)):
        raise RuntimeError('Native mission validation failed: '+str(response))


def _finish_assembly():
    validate_companion_talk_templates(assembly['heroes'])
    missions=make_missions(assembly['heroes'],assembly['evidence'],assembly['sequences'])
    if len(assembly['sequences'])!=18: raise RuntimeError('Full route requires exactly eighteen authored scenes')
    for chapter,mission in missions.items():
        _validate_native_mission(mission)
        save(mission)
        report.setdefault('missions',{})[str(chapter)]={'asset':mission.get_path_name(),
            'mission_id':str(mission.mission_id),'native_definition_valid':True,
            'companions':_encode(mission.protagonist_companions)}
    save(table)
    assert_sources_unchanged()
    report['source_memory_after']={p:_source_signature(obj) for p,obj in source_objects.items()}
    report['source_memory_unchanged']=report['source_memory_after']==source_memory
    report['status']='authored_and_navigation_checked_gameplay_unqualified'
    report['phase']='complete'
    _write_report()
    _stop_assembly()


def _stop_assembly():
    global assembly
    if assembly and assembly.get('handle') is not None:
        unreal.unregister_slate_post_tick_callback(assembly['handle'])
        assembly['handle']=None
    if assembly:
        assembly['context']=None; assembly['nav_volume']=None
        assembly['phase']='stopped'
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)


def _assembly_tick(delta_seconds):
    if not assembly or assembly.get('busy') or assembly.get('phase')=='stopped': return
    assembly['busy']=True
    try:
        _stopped()
        if time.monotonic()-assembly['started']>1200: raise RuntimeError('Full authoring exceeded its bounded twenty-minute deadline')
        if assembly['phase']=='assemble':
            _assemble_current_map()
        elif assembly['phase']=='navigation':
            if world()!=assembly['context']['world']: raise RuntimeError('Editor world changed during navigation authoring')
            if time.monotonic()-assembly['nav_started']>180: raise RuntimeError('Navigation build did not settle in three minutes')
            if unreal.SovAurelionNavigationLibrary.is_navigation_being_built_or_locked(world()):
                assembly['nav_idle_ticks']=0
            else:
                assembly['nav_idle_ticks']+=1
            if assembly['nav_idle_ticks']>=5 and time.monotonic()-assembly['nav_started']>=1:
                _validate_navigation(); assert_sources_unchanged()
                if not editor.save_current_level(): raise RuntimeError('Could not save assembled mission map')
                chapter=assembly['chapter']; report['maps'][str(chapter)]['saved']=True
                report['saved'].append(MAPS[chapter]); _write_report()
                # Drop all actors, components and the current UWorld before the next editor map load.
                assembly['context']=None; assembly['nav_volume']=None
                if chapter==12:
                    assembly['chapter']=13; assembly['phase']='assemble'
                else: assembly['phase']='finish'
        elif assembly['phase']=='finish':
            _finish_assembly()
    except Exception:
        report['status']='failed'; report['error']=traceback.format_exc()
        unreal.log_error(report['error']); _write_report(); _stop_assembly()
    finally:
        if assembly: assembly['busy']=False


def start():
    """Explicit full-authoring entrypoint. Importing this module performs no assembly."""
    global assembly
    if assembly and assembly.get('phase')!='stopped': raise RuntimeError('An authoring transaction is already active')
    _stopped()
    OUT.mkdir(parents=True,exist_ok=True)
    try:
        for module_dir in (HERE/'enemies/Scripts/Editor',HERE/'scanner/Scripts/Editor'):
            if module_dir.is_dir() and str(module_dir) not in sys.path: sys.path.insert(0,str(module_dir))
        for api_name in ('SovAurelionMedicalCache','SovAurelionSupportPresentation','SovAurelionSweepScanner',
                         'SovAurelionNavigationLibrary','SovAurelionEnemyAuthoringLibrary','SovAurelionDeparturePresentation',
                         'SovAurelionSceneValidationLibrary','SovAurelionNavigationProfileLibrary','SovWeaponHUDAuthoringLibrary'):
            if not hasattr(unreal,api_name): raise RuntimeError('Required native batch has not been loaded: '+api_name)
        capture_source_guards()
        prepare_maps(); seed_table()
        from setup_aurelion_enemy_roles import build_enemy_assets
        from setup_aurelion_scanners import configure_network_controller
        enemies=build_enemy_assets(OUT)
        # Compile all enemy/controller classes before their placed actors are captured in director arrays.
        scanner_controller=configure_network_controller(enemies,OUT)
        heroes=make_profiles(); evidence=make_evidence(); missions=_mission_shells(heroes)
        assembly={'phase':'assemble','chapter':12,'started':time.monotonic(),'busy':False,
                  'heroes':heroes,'evidence':evidence,'missions':missions,'enemies':enemies,
                  'scanner_controller':scanner_controller,
                  'sequences':{},'context':None,'handle':None,'nav_volume':None}
        report['status']='assembling'; report['phase']='M12'; report['profiles']=_encode(heroes)
        _write_report()
        unreal.EditorPythonScripting.set_keep_python_script_alive(True)
        assembly['handle']=unreal.register_slate_post_tick_callback(_assembly_tick)
        return assembly
    except Exception:
        report['status']='failed'; report['error']=traceback.format_exc()
        unreal.log_error(report['error']); _write_report()
        _stop_assembly()
        raise


if __name__ == '__main__':
    start()
