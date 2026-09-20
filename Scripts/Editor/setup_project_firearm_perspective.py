"""Bind the editor-repaired firearm base and project-owned weapon consumers.
Requires the two reviewed ResolvedFirstPerson transition graph edits first.
"""
import unreal, os, json, re
from pathlib import Path
OUT=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
PREFIX='/NarrativePro/Pro/Core/Character/Biped/Animation/ABP/Overlays/Weapons/'
DEST='/Game/Characters/Animation/Firearms/'
PAIRS=[(PREFIX+'ABP_Biped_Overlay_FireArmBase',DEST+'ABP_SovFirearmBase'),(PREFIX+'ABP_Biped_Overlay_Rifle',DEST+'ABP_SovRifle'),(PREFIX+'ABP_Biped_Overlay_DualFirearm',DEST+'ABP_SovDualFirearm'),('/NarrativePro/Pro/Core/BP/WeaponVisuals/Blueprints/NWV_Cinderline','/Game/Weapons/Visuals/BP_SovCinderlineVisual'),('/NarrativePro/Pro/Core/BP/WeaponVisuals/Blueprints/BP_Staccato_Visual','/Game/Weapons/Visuals/BP_SovStaccatoVisual')]
def cdo(bp):return unreal.get_default_object(bp.generated_class())
def export(bp,name):
 t=unreal.AssetExportTask();f=OUT/name
 for k,v in dict(object=bp,exporter=unreal.ObjectExporterT3D(),filename=str(f),automated=True,prompt=False,selected=False,replace_identical=True).items():t.set_editor_property(k,v)
 assert unreal.Exporter.run_asset_export_task(t)
 raw=f.read_bytes();return raw.decode('utf-16' if raw.startswith((b'\xff\xfe',b'\xfe\xff')) else 'utf-8-sig')
def run():
 report={'saved':[]}
 try:
  assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
  sources=[unreal.load_asset(a) for a,b in PAIRS]
  for source in sources:cdo(source)
  # Warm existing owned Blueprint dependencies before the mutation baseline.
  for _,target in PAIRS:
   existing=unreal.load_asset(target)
   if existing:unreal.BlueprintEditorLibrary.compile_blueprint(existing)
  unreal.SystemLibrary.collect_garbage()
  before=[unreal.SovBlueprintAuthoringLibrary.fingerprint_blueprint(x) for x in sources]
  base=unreal.load_asset(PAIRS[0][1]);assert base
  txt=export(base,'repaired-base-before-binding.t3d')
  guid=re.search(r'NewVariables\(\d+\)=\(VarName="ResolvedFirstPerson",VarGuid=([A-F0-9]+),VarType=\(PinCategory="bool"\)',txt).group(1)
  # Deleted editor objects may remain in the transaction buffer; graph membership is authoritative.
  for index in (4,1):
   start=txt.index('Begin Object Name="Transition"',txt.index(f'Begin Object Name="AnimStateTransitionNode_{index}"',txt.index('Begin Object Name="SM_FireArm_3P"')))
   end=txt.index('GraphGuid=',start);block=txt[start:end]
   members=re.findall(r'Nodes\(\d+\)="([^\n]+)',block)
   assert any("K2Node_VariableGet_0" in x for x in members)
   obsolete='K2Node_PropertyAccess_2' if index==4 else 'K2Node_PropertyAccess_0'
   assert not any(obsolete in x or 'EnumEquality' in x for x in members)
  old=cdo(base).get_editor_property('gameplay_tag_property_map')
  reload_guid=re.search(r'NewVariables\(\d+\)=\(VarName="Reloading",VarGuid=([A-F0-9]+)',txt).group(1)
  text=f'(PropertyMappings=((TagToMap=(TagName="Narrative.State.Weapon.Reloading"),PropertyName="Reloading",PropertyGuid={reload_guid})))';row=f'(TagToMap=(TagName="Camera.Perspective.FirstPerson"),PropertyName="ResolvedFirstPerson",PropertyGuid={guid})'
  if 'ResolvedFirstPerson' not in text:text=text[:-2]+','+row+'))'
  mapping=type(old)();assert mapping.import_text(text)
  base.modify();cdo(base).set_editor_property('gameplay_tag_property_map',mapping)
  copies=[base]
  for source,target in PAIRS[1:]:
   copies.append(unreal.load_asset(target) if unreal.EditorAssetLibrary.does_asset_exist(target) else unreal.EditorAssetLibrary.duplicate_asset(source,target))
  items=[unreal.load_asset('/Game/Items/Weapons/WI_'+name) for name in ('Cinderline','Staccato')]
  for item,source,visual in zip(items,sources[3:],copies[3:]):
   assert cdo(item).get_editor_property('weapon_visual_class') in (source.generated_class(),visual.generated_class())
   item.modify();cdo(item).modify();cdo(item).set_editor_property('weapon_visual_class',visual.generated_class())
  result=unreal.SovBlueprintAuthoringLibrary.remap_project_blueprint_references(copies+items,sources,copies)
  report['compilation']=str(result.get_editor_property('report'));assert result.get_editor_property('succeeded'),report['compilation']
  # Reparented children retain their old CDO overrides; replace only this audited map.
  for child in copies[1:3]:
   child.modify();cdo(child).modify()
   inherited=type(mapping)();assert inherited.import_text(text)
   cdo(child).set_editor_property('gameplay_tag_property_map',inherited)
  result=unreal.SovBlueprintAuthoringLibrary.remap_project_blueprint_references(copies+items,sources,copies)
  report['child_compilation']=str(result.get_editor_property('report'));assert result.get_editor_property('succeeded')
  for child in copies[1:3]:assert PAIRS[0][1]+'.ABP_SovFirearmBase_C' in export(child,child.get_name()+'.t3d').split('ParentClass=')[-1].split('\n')[0]
  for visual in copies[3:]:
   for field in ('default_weapon_anim_layer','weapon1p_anim_layer'):assert cdo(visual).get_editor_property(field)==copies[1].generated_class()
   assert cdo(visual).get_editor_property('dual_wield_weapon_anim_layer')==copies[2].generated_class()
  after=[unreal.SovBlueprintAuthoringLibrary.fingerprint_blueprint(x) for x in sources]
  report['source_fingerprints_before']=before;report['source_fingerprints_after']=after
  assert before==after,'Vendor source changed'
  report['tag_map']=cdo(base).get_editor_property('gameplay_tag_property_map').export_text()
  assert 'PropertyName="Reloading"' in report['tag_map'] and 'PropertyName="ResolvedFirstPerson"' in report['tag_map']
  report['child_maps']={child.get_name():cdo(child).get_editor_property('gameplay_tag_property_map').export_text() for child in copies[1:3]}
  assert all(value==report['tag_map'] for value in report['child_maps'].values())
  for bp in copies+items:
   assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
   report['saved'].append(bp.get_path_name())
  report['status']='saved; fresh runtime qualification required'
 except Exception as e:
  report['error']=str(e);unreal.log_error('FIREARM_REPAIR '+str(e))
 finally:(OUT/'firearm-repair.json').write_text(json.dumps(report,indent=2))
run()






