"""Replace twelve stretched crate presentations with fitted custom cover meshes."""
import json
import os
from pathlib import Path
import runpy
import shutil
import time
import unreal
persist=bool(globals().get('PERSIST',False))
root=Path(unreal.Paths.project_dir()).resolve(); out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
checks=runpy.run_path(str(root/'Scripts/Editor/check_z01_cover.py'))
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(); assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem); original=list(subsystem.get_all_level_actors()); by_label={a.get_actor_label():a for a in original}
assert len(original)==1929
source=root/'Art/Source/Aurelion/CoverKit'; baseline=json.loads((source/'placement-baseline.json').read_text()); destination='/Game/Aurelion/Environment/ArchitectureKit'
targets=[by_label[row['actor']] for row in baseline]; unaffected=[a for a in original if a not in targets]
before=helpers['snapshot_actor_state'](unaffected); target_before={a.get_actor_label():a.get_actor_transform() for a in targets}
materials={key:destination+'/Materials/M_AurelionKit_'+value for key,value in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_ChannelShadow':'Reveal','M_Aurelion_AncientGold':'Gold'}.items()}
meshes={spec['asset']:helpers['import_owned_mesh'](spec,source,destination+'/Meshes',materials) for spec in json.loads((source/'manifest.json').read_text())['modules']}
for row in baseline:
    a=by_label[row['actor']]; c=a.static_mesh_component; assert c.static_mesh.get_path_name()==row['mesh']
    assert c.get_world_transform().export_text()==row['transform'],'Cover moved since baseline audit'
    a.modify(); c.modify(); c.set_static_mesh(meshes['SM_Aurelion_KIT_'+checks['variant'](row['actor'])]); c.set_editor_property('override_materials',[])
    a.set_actor_scale3d(unreal.Vector(1,1,1)); c.set_collision_profile_name('BlockAll'); c.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
    old=target_before[row['actor']]; new=a.get_actor_transform()
    assert new.translation==old.translation and new.rotation==old.rotation,'Only target scale may change'
assert helpers['snapshot_actor_state'](unaffected)==before,'Unrelated actor state changed'
geometry=checks['check_cover'](world,original)
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap'); assert editor.save_current_level()
(out/'cover-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',geometry=geometry,
    changes='Twelve existing cover actors use custom meshes at unit scale; locations/rotations retained. Upper coffer skirt fills its existing 4.216 cm support gap.',
    qualification='Authoring evidence only; live encounter and performance acceptance pending'),indent=2))
editor.editor_set_game_view(True)
capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
capture=capture.replace("('entry',unreal.Vector(p.x,p.y,p.z+165),unreal.Rotator(yaw=90),90)","('cover-lane',unreal.Vector(-7000,-15700,165),unreal.Rotator(pitch=-5,yaw=120),80)")
capture=capture.replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('cover-stack',unreal.Vector(-7060,-14500,165),unreal.Rotator(pitch=0,yaw=63),75)")
exec(compile("p=by_label['Z01_Entry_StandIn']"+capture,'z01_cover_capture','exec'))
