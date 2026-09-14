"""Fit measured paving over Z02 floor while retaining all existing collision."""
import json
import os
from pathlib import Path
import runpy
import shutil
import time
import unreal
persist=bool(globals().get('PERSIST',False)); root=Path(unreal.Paths.project_dir()).resolve(); out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(); assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem); original=list(subsystem.get_all_level_actors()); by_label={a.get_actor_label():a for a in original}
assert len(original)==1929 and not any(a.get_actor_label().startswith('KIT_Z02_Paving_') for a in original)
before=helpers['snapshot_actor_state'](original)
source=root/'Art/Source/Aurelion/Z02PavingKit'; fit=json.loads((source/'floor-fit.json').read_text()); old=by_label['Cube2']
assert old.get_actor_transform().export_text()==fit['source']['transform'] and old.static_mesh_component.static_mesh.get_path_name()==fit['source']['mesh']
destination='/Game/Aurelion/Environment/ArchitectureKit'
materials={key:destination+'/Materials/M_AurelionKit_'+value for key,value in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_Basalt':'PavingBasalt','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal'}.items()}
meshes={s['asset']:helpers['import_owned_mesh'](s,source,destination+'/Meshes',materials) for s in json.loads((source/'manifest.json').read_text())['modules']}
for suffix in ('PavingIvory_4m','PavingRoute_4m'):meshes['SM_Aurelion_KIT_'+suffix]=unreal.load_asset(destination+'/Meshes/SM_Aurelion_KIT_'+suffix)
for ci,col in enumerate(fit['columns']):
    for ri,row in enumerate(fit['rows']):
        suffix='Z02Paving'+col['kind'].title()+'_Short' if row['short'] else col['suffix']; mesh=meshes['SM_Aurelion_KIT_'+suffix]
        b=mesh.get_bounds(); top=b.origin.z+b.box_extent.z
        a=subsystem.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(col['x'],row['y'],fit['surface_z']-top))
        a.set_actor_label(f'KIT_Z02_Paving_{ci:02}_{ri:02}'); a.set_folder_path('Aurelion/CustomArchitecture/Z02/Paving')
        c=a.static_mesh_component; c.set_static_mesh(mesh); c.set_collision_profile_name('NoCollision'); c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION); a.set_actor_enable_collision(False)
for label in ('Cube2','Z02__GoldChannel_01','Z02__GoldChannel_02'):
    c=by_label[label].static_mesh_component; c.modify(); c.set_visibility(False,False); c.set_hidden_in_game(True,False)
legacy=by_label['Aurelion_Art_M12_Z02_11_859d0a'].get_component_by_class(unreal.InstancedStaticMeshComponent)
assert legacy.get_instance_count()==14 and legacy.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
for i in range(14):
    p=legacy.get_instance_transform(i,world_space=True).translation
    assert abs(p.x+7000)<.01 and -9700<p.y<-8300
legacy.modify(); legacy.set_visibility(False,False); legacy.set_hidden_in_game(True,False)
assert helpers['snapshot_actor_state'](original)==before,'Existing transforms/collision changed'
geometry=runpy.run_path(str(root/'Scripts/Editor/check_z02_paving.py'))['check_paving'](world,list(subsystem.get_all_level_actors()))
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap'); assert editor.save_current_level()
(out/'z02-paving-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',geometry=geometry,original_actor_count=len(original),changes='45 fitted paving actors; old floor and decorative guides hidden; original transforms and collision retained'),indent=2))
editor.editor_set_game_view(True)
capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
capture=capture.replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('paving-detail',unreal.Vector(-7000,-9050,180),unreal.Rotator(pitch=-40,yaw=90),80)").replace('z01-','z02-')
exec(compile("p=by_label['Z02_Entry_StandIn']"+capture,'z02_paving_capture','exec'))
