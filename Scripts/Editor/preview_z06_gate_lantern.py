"""Gate housing and dedicated modeled lanterns; unsaved art fit by default."""
from pathlib import Path
import json,os,runpy,shutil,time
import unreal
root=Path(unreal.Paths.project_dir())
housing_script=(root/'Scripts/Editor/preview_z06_gate_housing.py').read_text()
setup,capture=housing_script.split('editor.editor_set_game_view(True)',1)
exec(compile(setup,'gate_housing_setup','exec'),globals())
fit=json.loads((root/'Art/Source/Aurelion/GateLanternKit/lighting-fit.json').read_text())
changed=[]
for side,sign in [('West',-1),('East',1)]:
    for i in range(4):
        light=by_label[f'KIT_Z06_Uplight_{side}_{i:02}_Light'];changed.append(light)
        p=light.get_actor_location();light.modify();c=light.get_component_by_class(unreal.SpotLightComponent);c.modify()
        light.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(p,unreal.Vector(sign*fit['uplight_target_x'],p.y,50)),False)
        c.set_intensity(fit['uplight_lumens'])
key=by_label['ENVL_Z06_Key_01'].get_component_by_class(unreal.RectLightComponent);key.modify();key.set_intensity(fit['key_01_lumens'])
source=root/'Art/Source/Aurelion/GateLanternKit'
materials={key:destination+'/Materials/M_AurelionKit_'+value for key,value in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_ChannelShadow':'Reveal','M_Aurelion_AncientGold':'Gold','M_Aurelion_UplightLens':'UplightLens'}.items()}
mesh=helpers['import_owned_mesh'](json.loads((source/'manifest.json').read_text())['modules'][0],source,destination+'/Meshes',materials)
for row in fit['lanterns']:
    p=unreal.Vector(*row['location']);name='KIT_Z06_GateLantern_'+row['name']
    actor=subsystem.spawn_actor_from_class(unreal.StaticMeshActor,p);actor.set_actor_label(name);actor.set_folder_path('Aurelion/CustomArchitecture/Z06/Refuge')
    c=actor.static_mesh_component;c.set_static_mesh(mesh);c.set_collision_profile_name('NoCollision');c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);actor.set_actor_enable_collision(False)
    light=subsystem.spawn_actor_from_class(unreal.RectLight,p+unreal.Vector(0,0,fit['light_offset_z']),unreal.Rotator(pitch=-90,yaw=90));light.set_actor_label(name+'_Light');light.set_folder_path('Aurelion/CustomArchitecture/Z06/Refuge')
    c=light.get_component_by_class(unreal.RectLightComponent);c.set_mobility(unreal.ComponentMobility.MOVABLE);c.set_editor_property('intensity_units',unreal.LightUnits.LUMENS);c.set_intensity(row['lumens']);c.set_attenuation_radius(fit['radius']);c.set_source_width(fit['source_width']);c.set_source_height(fit['source_height']);c.set_light_color(unreal.LinearColor(*fit['color'],1));c.set_cast_shadows(True)
preserved=[a for a in original if a not in changed]
assert helpers['snapshot_actor_state'](preserved)=={a.get_path_name():before[a.get_path_name()] for a in preserved}
actors=list(subsystem.get_all_level_actors());assert len(actors)==2842
gate=runpy.run_path(str(root/'Scripts/Editor/check_z06_rescue_gate.py'))['check_z06_rescue_gate'](world,actors)
assembly=runpy.run_path(str(root/'Scripts/Editor/check_z06_gate_assembly.py'))['check_z06_gate_assembly'](world,actors)
persist=bool(globals().get('SAVE_GATE_ASSEMBLY',False))
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'gate-lantern-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',actor_count=len(actors),fit=fit,gate=gate,assembly=assembly,qualification='Dedicated fixtures and gate housing; original geometry and door motion preserved, live visual acceptance pending.'),indent=2))
exec(compile('editor.editor_set_game_view(True)'+capture,'gate_lantern_capture','exec'),globals())
