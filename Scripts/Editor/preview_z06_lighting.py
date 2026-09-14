"""Balance local keys with modeled upward-facing coffer lights; preview by default."""
import json,os,runpy,shutil,time
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);persist=bool(globals().get('PERSIST',False))
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);original=list(subsystem.get_all_level_actors());by_label={a.get_actor_label():a for a in original};assert len(original)==2797
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));before=helpers['snapshot_actor_state'](original)
fit=json.loads((root/'Art/Source/Aurelion/Z06Lighting/lighting-fit.json').read_text());mesh=unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Meshes/SM_Aurelion_KIT_PierUplight');assert mesh
for row in fit['baseline']:
    c=by_label[row['actor']].get_component_by_class(unreal.RectLightComponent)
    assert c.get_world_transform().export_text()==row['transform'] and c.get_editor_property('intensity')==row['intensity'] and c.get_editor_property('attenuation_radius')==row['radius']
    c.modify();c.set_intensity(fit['key_lumens']);c.set_attenuation_radius(fit['key_radius'])
for side,sign,yaw in [('West',-1,-90),('East',1,90)]:
    for i,y in enumerate(fit['stations']):
        name=f'KIT_Z06_Uplight_{side}_{i:02}';a=subsystem.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(sign*fit['fixture_x'],y,fit['fixture_z']),unreal.Rotator(yaw=yaw));a.set_actor_label(name);a.set_folder_path('Aurelion/CustomArchitecture/Z06/Lighting')
        c=a.static_mesh_component;c.set_static_mesh(mesh);c.set_collision_profile_name('NoCollision');c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);a.set_actor_enable_collision(False)
        p=unreal.Vector(sign*fit['light_x'],y,fit['light_z']);rotation=unreal.MathLibrary.find_look_at_rotation(p,unreal.Vector(sign*fit['target_x'],y,fit['target_z']))
        light=subsystem.spawn_actor_from_class(unreal.SpotLight,p,rotation);light.set_actor_label(name+'_Light');light.set_folder_path('Aurelion/CustomArchitecture/Z06/Lighting')
        c=light.get_component_by_class(unreal.SpotLightComponent);c.set_mobility(unreal.ComponentMobility.MOVABLE);c.set_editor_property('intensity_units',unreal.LightUnits.LUMENS);c.set_editor_property('use_inverse_squared_falloff',True);c.set_intensity(fit['lumens']);c.set_attenuation_radius(fit['attenuation_radius']);c.set_inner_cone_angle(fit['inner_cone']);c.set_outer_cone_angle(fit['outer_cone']);c.set_source_radius(fit['source_radius']);c.set_light_color(unreal.LinearColor(*fit['color'],1));c.set_cast_shadows(True)
assert helpers['snapshot_actor_state'](original)==before
geometry=runpy.run_path(str(root/'Scripts/Editor/check_z06_lighting.py'))['check_z06_lighting'](world,list(subsystem.get_all_level_actors()))
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'z06-lighting-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',geometry=geometry),indent=2))
editor.editor_set_game_view(True)
capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
capture=capture.replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('lighting-detail',unreal.Vector(-600,8000,-435),unreal.Rotator(pitch=10,yaw=-145),90)").replace('z01-','z06-')
exec(compile("p=by_label['Z06_Entry_StandIn']"+capture,'z06_lighting_capture','exec'))
