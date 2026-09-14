"""Restore the omitted interior wall envelopes and bind actual climb surfaces."""
from pathlib import Path
import json,os,runpy,shutil,time
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);original=list(subsystem.get_all_level_actors());by_label={a.get_actor_label():a for a in original};assert len(original)==3140
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));before=helpers['snapshot_actor_state'](original)
source=root/'Art/Source/Aurelion/Z08WallKit';dest='/Game/Aurelion/Environment/ArchitectureKit'
materials={k:dest+'/Materials/M_AurelionKit_'+v for k,v in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal'}.items()}
shutil.copy2(root/'Content/Aurelion/Environment/ArchitectureKit/Meshes/SM_Aurelion_KIT_Z08WallAssembly.uasset',out/'SM_Aurelion_KIT_Z08WallAssembly-before.uasset')
meshes={s['asset']:helpers['import_owned_mesh'](s,source,dest+'/Meshes',materials) for s in json.loads((source/'manifest.json').read_text())['modules']}
bound=[]
for group,mesh_name in [('E3','SM_Aurelion_KIT_Z06ClimbPanel'),('E4','SM_Aurelion_KIT_Z08WallAssembly')]:
    routes=[a for a in original if isinstance(a,unreal.SovAurelionWallRoute) and str(a.get_editor_property('route_id'))=='Aurelion.'+group+'.WallEntry'];assert len(routes)==1
    surfaces=[c for a in original for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh and c.static_mesh.get_name()==mesh_name and c.get_editor_property('visible')]
    assert len(surfaces)==1,(group,[c.get_path_name() for c in surfaces])
    route=routes[0];assert not route.get_editor_property('presentation_surfaces');route.modify();route.set_editor_property('presentation_surfaces',surfaces)
    bound.append(dict(route=route.get_actor_label(),surface=surfaces[0].get_path_name(),mesh=mesh_name))
assert helpers['snapshot_actor_state'](original)==before
geometry=runpy.run_path(str(root/'Scripts/Editor/check_z08_walls.py'))['check_z08_walls'](world,original)
shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'wall-contact-authoring.json').write_text(json.dumps(dict(status='saved',bindings=bound,geometry=geometry,preserved_actor_states=len(original)),indent=2))
editor.editor_set_game_view(True)
capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
capture=capture.replace("('entry',unreal.Vector(p.x,p.y,p.z+165),unreal.Rotator(yaw=90),90)","('climb-wall',unreal.Vector(2100,21800,-950),unreal.Rotator(pitch=0,yaw=170),80)")
capture=capture.replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('refuge-walls',unreal.Vector(-2450,21600,-880),unreal.Rotator(pitch=-5,yaw=110),90)").replace('z01-','z08-')
exec(compile("p=by_label['Z08_Entry_StandIn']"+capture,'wall_coverage_review','exec'))
