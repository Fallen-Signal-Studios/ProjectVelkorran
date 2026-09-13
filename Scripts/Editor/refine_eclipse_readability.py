"""Single visible Linkbound mesh and bounded existing encounter-light adjustment."""
import json
import shutil
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir()).resolve()
out=root/'Saved/Validation/Aurelion/EclipseAnimation'
out.mkdir(parents=True,exist_ok=True)
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().startswith('/Game/Aurelion/Maps/L_Aurelion_M12.')
report={'status':'authoring','lights':[],'visual_qualified':False}
appearance=unreal.load_asset('/Game/Aurelion/Art/Characters/Appearances/CA_AurelionLinkbound')
attrs=appearance.get_editor_property('character_attributes')
assert attrs.get_editor_property('base_mesh').get_name()=='SK_Parasit'
targets={'ENVL_Z06_Key_00','ENVL_Z06_Key_01','ENVL_Z06_Key_02',
         'ENVL_Z07_Key_00','ENVL_Z07_Key_01',
         'ENVL_Z08_Key_00','ENVL_Z08_Key_01','ENVL_Z08_Key_02'}
lights=[a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
        if a.get_actor_label() in targets]
assert len(lights)==len(targets) and all(isinstance(a,unreal.RectLight) for a in lights)
# Preserve the exact user-edited map, not merely the version in Git.
for relative in ('Content/Aurelion/Maps/L_Aurelion_M12.umap',
                 'Content/Aurelion/Art/Characters/Appearances/CA_AurelionLinkbound.uasset'):
    source=root/relative
    backup=out/'before-readability'/relative
    backup.parent.mkdir(parents=True,exist_ok=True)
    if not backup.exists(): shutil.copy2(source,backup)
report['appearance_before']=attrs.export_text()
attrs.set_editor_property('hide_base_mesh',False)
attrs.set_editor_property('meshes',{})
appearance.set_editor_property('character_attributes',attrs)
assert unreal.EditorAssetLibrary.save_loaded_asset(appearance,only_if_is_dirty=False)
report['appearance_after']=attrs.export_text()
for actor in lights:
    light=actor.get_component_by_class(unreal.RectLightComponent)
    report['lights'].append({'label':actor.get_actor_label(),
        'before':light.get_editor_property('intensity'),'after':1800.,
        'units':str(light.get_editor_property('intensity_units'))})
    light.set_intensity(1800.)
assert editor.save_current_level()
report['status']='saved_requires_rendered_review'
(out/'readability-authoring.json').write_text(json.dumps(report,indent=2),encoding='utf8')
