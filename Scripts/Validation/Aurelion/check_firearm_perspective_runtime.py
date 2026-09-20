"""Fresh PIE controlled perspective-to-linked-animation checks; not visual pose acceptance."""
import unreal,time,os,json,traceback
from pathlib import Path
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])/'firearm-runtime.json'
report={'status':'starting','samples':[],'defaults':{}}
for name in ('ABP_SovFirearmBase','ABP_SovRifle','ABP_SovDualFirearm'):
 bp=unreal.load_asset('/Game/Characters/Animation/Firearms/'+name)
 text=unreal.get_default_object(bp.generated_class()).get_editor_property('gameplay_tag_property_map').export_text()
 report['defaults'][name]=text
out.write_text(json.dumps(report,indent=2))
settings=unreal.GameUserSettings.get_game_user_settings();original=settings.get_settings_snapshot();settings.complete_accessibility_setup()
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
started=time.monotonic();phase='start';at=started;index=0;pawn=None;previous=None;wield_requested=False
modes=['FAR','FIRST_PERSON','BALANCED','FIRST_PERSON','CLOSE']
def tick(delta):
 global phase,at,index,pawn,previous,wield_requested
 try:
  if time.monotonic()-started>100:raise RuntimeError('timeout '+phase)
  editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
  if phase=='start':editor.editor_request_begin_play();phase='ready';return
  world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
  pawn=unreal.GameplayStatics.get_player_pawn(world,0) if world else None
  if not pawn or not pawn.is_character_ready() or pawn.is_character_pending_load():return
  if not wield_requested:
   inventory=pawn.get_component_by_class(unreal.NarrativeInventoryComponent)
   item=inventory.find_item_of_class(unreal.load_class(None,'/Game/Items/Weapons/WI_Cinderline.WI_Cinderline_C'),False)
   assert item is not None,'Cinderline missing from starting kit'
   slot=unreal.GameplayTagLibrary.get_tag_name(item.get_editor_property('current_slot'))
   state=unreal.WeaponWieldState()
   equip=unreal.GameplayTagContainer();assert equip.import_text('(GameplayTags=((TagName="%s")))'%slot)
   hands=unreal.GameplayTagContainer();assert hands.import_text('(GameplayTags=((TagName="Narrative.Equipment.WieldSlot.Mainhand")))')
   state.set_editor_property('equip_slots',equip);state.set_editor_property('equip_weapons',[item]);state.set_editor_property('wield_slots',hands)
   pawn.set_wield_state(state);wield_requested=True
   return
  instances=[a for a in unreal.ObjectIterator(unreal.NarrativeAnimInstance) if a.get_class().get_path_name().startswith('/Game/Characters/Animation/Firearms/') and isinstance(a.get_outer(),unreal.SkeletalMeshComponent) and 'UEDPIE_' in a.get_path_name() and a.get_character_ref()==pawn]
  if not instances:return
  if previous is None:previous=pawn.get_editor_property('CameraStyle')
  if phase=='ready':
   pawn.call_method('SetCameraMode',(getattr(type(previous),modes[index]),));at=time.monotonic();phase='sample';return
  if phase=='sample' and time.monotonic()-at>1:
   state=pawn.get_component_by_class(unreal.SovCameraControlComponent).get_camera_state()
   first=state.style==unreal.SovCameraStyle.FIRST_PERSON
   rows=[{'class':a.get_class().get_path_name(),'first':a.get_editor_property('ResolvedFirstPerson'),'map':a.get_editor_property('gameplay_tag_property_map').export_text()} for a in instances]
   report['samples'].append({'requested':modes[index],'resolved':str(state.style),'animations':rows,'pawn':pawn.get_class().get_name()})
   assert all(r['first']==first for r in rows),rows
   assert all('PropertyName="Reloading"' in r['map'] and 'PropertyName="ResolvedFirstPerson"' in r['map'] for r in rows),rows
   index+=1
   if index==len(modes):report['status']='passed_controlled_linked_perspective';phase='end'
   else:phase='ready'
 except Exception:report.update(status='failed',error=traceback.format_exc());phase='end'
 if phase=='end':
  if pawn and previous is not None:pawn.call_method('SetCameraMode',(previous,))
  settings.apply_settings_snapshot(original);out.write_text(json.dumps(report,indent=2));unreal.unregister_slate_post_tick_callback(handle);unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play();unreal.EditorPythonScripting.set_keep_python_script_alive(False)
handle=unreal.register_slate_post_tick_callback(tick)


