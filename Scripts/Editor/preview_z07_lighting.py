"""Six modeled gallery uplights; existing lights and mission actors retained."""
from pathlib import Path
import json,os,runpy,shutil,time
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);original=list(subsystem.get_all_level_actors());by_label={a.get_actor_label():a for a in original};assert len(original)==2923
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));before=helpers['snapshot_actor_state'](original)
for i in range(2):
    c=by_label[f'ENVL_Z07_Key_{i:02}'].get_component_by_class(unreal.RectLightComponent)
    assert not c.get_editor_property('cast_shadows');c.modify();c.set_cast_shadows(True)
fit=json.loads((root/'Art/Source/Aurelion/Z07Lighting/lighting-fit.json').read_text());mesh=unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Meshes/SM_Aurelion_KIT_PierUplight');assert mesh
for i in range(2):
    c=by_label[f'ENVL_Z07_Key_{i:02}'].get_component_by_class(unreal.RectLightComponent)
    assert c.get_editor_property('source_width')==240 and c.get_editor_property('source_height')==160
    c.set_editor_property('source_width',fit['key_source_width']);c.set_editor_property('source_height',fit['key_source_height'])
for side,sign,yaw in [('West',-1,-90),('East',1,90)]:
    for i,(x,y) in enumerate(fit['stations']):
        name=f'KIT_Z07_Uplight_{side}_{i:02}';assert name not in by_label
        a=subsystem.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(sign*x,y,fit['fixture_z']),unreal.Rotator(yaw=yaw));a.set_actor_label(name);a.set_folder_path('Aurelion/CustomArchitecture/Z07/Lighting')
        c=a.static_mesh_component;c.set_static_mesh(mesh);c.set_collision_profile_name('NoCollision');c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);a.set_actor_enable_collision(False)
        p=unreal.Vector(sign*(x-fit['optical_offset']),y,fit['light_z'])
        light=subsystem.spawn_actor_from_class(unreal.SpotLight,p,unreal.Rotator(pitch=90));light.set_actor_label(name+'_Light');light.set_folder_path('Aurelion/CustomArchitecture/Z07/Lighting')
        c=light.get_component_by_class(unreal.SpotLightComponent);c.set_mobility(unreal.ComponentMobility.MOVABLE);c.set_editor_property('intensity_units',unreal.LightUnits.LUMENS);c.set_editor_property('use_inverse_squared_falloff',True)
        c.set_intensity(fit['lumens']);c.set_attenuation_radius(fit['attenuation_radius']);c.set_inner_cone_angle(fit['inner_cone']);c.set_outer_cone_angle(fit['outer_cone']);c.set_source_radius(fit['source_radius']);c.set_light_color(unreal.LinearColor(*fit['color'],1));c.set_cast_shadows(True)
        c.set_editor_property('indirect_lighting_intensity',1);c.set_editor_property('volumetric_scattering_intensity',0)
assert helpers['snapshot_actor_state'](original)==before
geometry=runpy.run_path(str(root/'Scripts/Editor/check_z07_lighting.py'))['check_z07_lighting'](world,list(subsystem.get_all_level_actors()))
persist=bool(globals().get('PERSIST',False))
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'z07-lighting-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',geometry=geometry,preserved_actor_states=len(original)),indent=2))
if not globals().get('SKIP_LIGHT_CAPTURE',False):
    editor.editor_set_game_view(True)
    capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
    capture=capture.replace("('entry',unreal.Vector(p.x,p.y,p.z+165),unreal.Rotator(yaw=90),90)","('entry-lighting',unreal.Vector(-1100,14200,-710),unreal.Rotator(pitch=8,yaw=65),90)")
    capture=capture.replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('coffer-lighting',unreal.Vector(0,15400,-650),unreal.Rotator(pitch=32,yaw=-90),90)").replace('z01-','z07-')
    exec(compile("p=by_label['Z07_Entry_StandIn']"+capture,'z07_lighting_capture','exec'))
