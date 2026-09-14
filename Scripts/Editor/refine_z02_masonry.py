"""Reimport owned perimeter masonry with recessed warm grout and fewer fine marks."""
import json,os,runpy,time,shutil
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);actors=list(subsystem.get_all_level_actors());by_label={a.get_actor_label():a for a in actors};assert len(actors)==2062
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));before=helpers['snapshot_actor_state'](actors)
destination='/Game/Aurelion/Environment/ArchitectureKit';path=destination+'/Materials/M_AurelionKit_StoneGrout'
grout=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
if not grout:
    grout=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_AurelionKit_StoneGrout',destination+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
    color=unreal.MaterialEditingLibrary.create_material_expression(grout,unreal.MaterialExpressionConstant3Vector);color.set_editor_property('constant',unreal.LinearColor(.30,.275,.23,1));assert unreal.MaterialEditingLibrary.connect_material_property(color,'',unreal.MaterialProperty.MP_BASE_COLOR)
    for prop,value in ((unreal.MaterialProperty.MP_ROUGHNESS,.88),(unreal.MaterialProperty.MP_METALLIC,0)):
        node=unreal.MaterialEditingLibrary.create_material_expression(grout,unreal.MaterialExpressionConstant);node.set_editor_property('r',value);assert unreal.MaterialEditingLibrary.connect_material_property(node,'',prop)
    unreal.MaterialEditingLibrary.recompile_material(grout);assert unreal.EditorAssetLibrary.save_loaded_asset(grout)
source=root/'Art/Source/Aurelion/Z02PerimeterKit'
materials={key:destination+'/Materials/M_AurelionKit_'+value for key,value in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal','M_Aurelion_StoneGrout':'StoneGrout'}.items()}
unreal.SystemLibrary.execute_console_command(world,'Interchange.FeatureFlags.Import.FBX 0')
changed=[]
for spec in json.loads((source/'manifest.json').read_text())['modules']:
    if 'Z02Perimeter_' not in spec['asset']:continue
    mesh=helpers['import_owned_mesh'](spec,source,destination+'/Meshes',materials);changed.append(mesh.get_path_name())
assert len(changed)==2 and helpers['snapshot_actor_state'](actors)==before
map_saved=False
if unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages():
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level();map_saved=True
(out/'masonry-refinement.json').write_text(json.dumps(dict(status='assets_saved',map_saved=map_saved,assets=changed,actor_count=len(actors),qualification='Owned mesh/material refinement; full visual and gameplay acceptance pending.'),indent=2))
editor.editor_set_game_view(True)
capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
capture=capture.replace("('entry',unreal.Vector(p.x,p.y,p.z+165),unreal.Rotator(yaw=90),90)","('masonry-comparison',unreal.Vector(-6920,-9220,170),unreal.Rotator(pitch=24,yaw=160),85)").replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('masonry-detail',unreal.Vector(-7900,-9200,180),unreal.Rotator(pitch=8,yaw=180),75)").replace('z01-','z02-')
exec(compile("p=by_label['Z02_Entry_StandIn']"+capture,'masonry_capture','exec'))
