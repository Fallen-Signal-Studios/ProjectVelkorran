"""Fit visible architectural uplights to reveal the south facade."""
import json
import os
from pathlib import Path
import runpy
import shutil
import time
import unreal
persist=bool(globals().get('PERSIST',False));root=Path(unreal.Paths.project_dir()).resolve();out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));checks=runpy.run_path(str(root/'Scripts/Editor/check_z02_facade_lighting.py'))
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);original=list(subsystem.get_all_level_actors());by_label={a.get_actor_label():a for a in original}
assert len(original)==2062 and not any(a.get_actor_label().startswith('KIT_Z02_FacadeLight_') for a in original)
before=helpers['snapshot_actor_state'](original)
fit=json.loads((root/'Art/Source/Aurelion/UplightKit/z02-facade-lighting.json').read_text())
mesh=unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Meshes/SM_Aurelion_KIT_PierUplight');assert mesh
for row in fit['fixtures']:
    x,y,z=row['position'];label='KIT_Z02_FacadeLight_'+row['name']
    a=subsystem.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(x,y,z),unreal.Rotator(yaw=180));a.set_actor_label(label);a.set_folder_path('Aurelion/CustomArchitecture/Z02/FacadeLighting')
    c=a.static_mesh_component;c.set_static_mesh(mesh);c.set_collision_profile_name('NoCollision');c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);a.set_actor_enable_collision(False)
    p=unreal.Vector(x,y-27.5,z+49);rotation=unreal.MathLibrary.find_look_at_rotation(p,unreal.Vector(x,y+28.5,row['target_z']))
    light=subsystem.spawn_actor_from_class(unreal.RectLight,p,rotation);light.set_actor_label(label+'_Light');light.set_folder_path('Aurelion/CustomArchitecture/Z02/FacadeLighting')
    c=light.get_component_by_class(unreal.RectLightComponent);c.set_mobility(unreal.ComponentMobility.MOVABLE);c.set_editor_property('intensity_units',unreal.LightUnits.LUMENS);c.set_intensity(row['lumens']);c.set_attenuation_radius(row['radius']);c.set_light_color(unreal.LinearColor(*fit['color'],1));c.set_source_width(fit['source_width']);c.set_source_height(fit['source_height'])
assert helpers['snapshot_actor_state'](original)==before
geometry=checks['check_lighting'](world,list(subsystem.get_all_level_actors()))
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'z02-facade-lighting.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',geometry=geometry,original_actor_count=len(original)),indent=2))
editor.editor_set_game_view(True)
capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
capture=capture.replace("('entry',unreal.Vector(p.x,p.y,p.z+165),unreal.Rotator(yaw=90),90)","('south-approach',unreal.Vector(-7000,-11800,175),unreal.Rotator(pitch=0,yaw=90),90)").replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('facade-detail',unreal.Vector(-8000,-10900,350),unreal.Rotator(pitch=12,yaw=80),85)").replace('z01-','z02-')
exec(compile("p=by_label['Z02_Entry_StandIn']"+capture,'z02_guard_capture','exec'))
