"""Attach physical plaques to existing signs and apply reviewed west-key settings."""
from pathlib import Path
import json,os,runpy,shutil,unreal
root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/RefugeSignKit';out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());assert len(actors)==3140;by_label={a.get_actor_label():a for a in actors}
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));before=helpers['snapshot_actor_state'](actors);old={r['actor']:r for r in json.loads((source/'presentation-baseline.json').read_text())};fit=json.loads((source/'presentation-fit.json').read_text());selected=set()
assert not json.loads((source/'coplanar-faces.json').read_text())['overlaps']
dest='/Game/Aurelion/Environment/ArchitectureKit';spec=json.loads((source/'manifest.json').read_text())['modules'][0];materials={k:dest+'/Materials/M_AurelionKit_'+v for k,v in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal'}.items()}
mesh=unreal.load_asset(dest+'/Meshes/'+spec['asset']) if globals().get('PERSIST_REFUGE_PRESENTATION',False) else helpers['import_owned_mesh'](spec,source,dest+'/Meshes',materials)
sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem);lib=unreal.SubobjectDataBlueprintFunctionLibrary
for name,x in [('ART_RefugeBaffle_WestSign',-2600),('ART_RefugeBaffle_EastSign',3050)]:
    a=by_label[name];t=a.get_component_by_class(unreal.TextRenderComponent);selected.add(a.get_path_name())
    assert a.get_actor_transform().export_text()==old[name]['transform'] and not a.get_components_by_class(unreal.StaticMeshComponent)
    a.modify();t.modify();t.set_world_size(fit['text_size']);t.set_vertical_alignment(unreal.VerticalTextAligment.EVRTA_TEXT_CENTER)
    a.set_actor_location(unreal.Vector(x,fit['text_y'],fit['text_center_z']),False,False)
    handles=sub.k2_gather_subobject_data_for_instance(a);parent=next(h for h in handles if lib.get_associated_object(lib.get_data(h))==a)
    handle,reason=sub.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=parent,new_class=unreal.StaticMeshComponent,conform_transform_to_parent=True))
    assert lib.is_handle_valid(handle),str(reason);assert sub.rename_subobject(handle,'RefugeSignMount')
    c=lib.get_associated_object(lib.get_data(handle));assert isinstance(c,unreal.StaticMeshComponent);c.modify();c.set_static_mesh(mesh);c.set_mobility(t.mobility)
    c.set_collision_profile_name('NoCollision');c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);a.set_actor_enable_collision(False)
    c.set_world_transform(unreal.Transform(location=unreal.Vector(x,fit['mount_y'],fit['mount_bottom_z']),rotation=unreal.Rotator(yaw=180),scale=unreal.Vector(1,1,1)),False,False)
a=by_label['ENVL_Z08_Key_02'];c=a.get_component_by_class(unreal.RectLightComponent);assert c.intensity==old[a.get_actor_label()]['light']['intensity'] and c.attenuation_radius==old[a.get_actor_label()]['light']['radius'];c.modify();c.set_intensity(fit['key_intensity']);c.set_attenuation_radius(fit['key_radius'])
after=helpers['snapshot_actor_state'](actors);assert {k:v for k,v in before.items() if k not in selected}=={k:v for k,v in after.items() if k not in selected}
result=runpy.run_path(str(root/'Scripts/Editor/check_refuge_presentation.py'))['check_refuge_presentation'](actors)
persist=bool(globals().get('PERSIST_REFUGE_PRESENTATION',False))
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'refuge-presentation-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',settings=result,preserved_other_actor_states=len(actors)-len(selected)),indent=2))
exec(compile((root/'Scripts/Editor/review_refuge_presentation.py').read_text(),'presentation_capture','exec'),globals())
