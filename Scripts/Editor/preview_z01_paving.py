"""Fit the complete Z01 paving cross-section; preview unless explicitly saved."""
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
snapshot=helpers['snapshot_actor_state']; import_mesh=helpers['import_owned_mesh']
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(); assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
original=list(subsystem.get_all_level_actors()); by_label={a.get_actor_label():a for a in original}
assert len(original)==1816
assert not any(a.get_actor_label().startswith('KIT_Z01_Paving_') for a in original)
before=snapshot(original)
floor=by_label['Z01_Floor']; old_art=by_label['aureliontarrikfloor']
assert floor.static_mesh_component.get_collision_enabled()==unreal.CollisionEnabled.QUERY_AND_PHYSICS
# Isolate the two existing floor surfaces. Covers/ramps are intentionally ignored here.
ignored=[a for a in original if a not in (floor,old_art)]; samples=[]
for x in (-8250,-8150,-7800,-7400,-7000,-6600,-6200,-5850,-5750):
    for y in [-17300+400*i for i in range(14)]:
        raw=unreal.SystemLibrary.line_trace_single(world,unreal.Vector(x,y,100),unreal.Vector(x,y,-50),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,True,ignored,unreal.DrawDebugTrace.NONE,True)
        hit=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
        assert hit.to_tuple()[0], (x,y,'No floor collision')
        z=hit.to_tuple()[5].z; assert abs(z)<.2,(x,y,z)
        samples.append(dict(x=x,y=y,z=z))
(out/'floor-height-probes.json').write_text(json.dumps(samples,indent=2))
source=root/'Art/Source/Aurelion/PavingKit'; destination='/Game/Aurelion/Environment/ArchitectureKit'
lib=unreal.MaterialEditingLibrary
for name,color,blend,rough in (('M_AurelionKit_PavingIvory',(.42,.40,.35),.12,.60),('M_AurelionKit_PavingBasalt',(.025,.028,.033),.06,.63)):
    path=destination+'/Materials/'+name
    material=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.EditorAssetLibrary.duplicate_asset(destination+'/Materials/M_AurelionKit_Ivory',path)
    assert material
    base=lib.get_material_property_input_node(material,unreal.MaterialProperty.MP_BASE_COLOR)
    assert isinstance(base,unreal.MaterialExpressionLinearInterpolate)
    constant=lib.get_inputs_for_material_expression(material,base)[0]
    assert isinstance(constant,unreal.MaterialExpressionConstant3Vector)
    constant.set_editor_property('constant',unreal.LinearColor(*color,1)); base.set_editor_property('const_alpha',blend)
    roughness=lib.get_material_property_input_node(material,unreal.MaterialProperty.MP_ROUGHNESS)
    assert isinstance(roughness,unreal.MaterialExpressionAdd)
    variation=lib.get_inputs_for_material_expression(material,roughness)[0]
    assert isinstance(variation,unreal.MaterialExpressionMultiply)
    roughness.set_editor_property('const_b',rough); variation.set_editor_property('const_b',.10)
    normal=lib.get_material_property_input_node(material,unreal.MaterialProperty.MP_NORMAL)
    assert isinstance(normal,unreal.MaterialExpressionLinearInterpolate)
    normal.set_editor_property('const_alpha',.12)
    lib.recompile_material(material); assert unreal.EditorAssetLibrary.save_loaded_asset(material)
materials={key:destination+'/Materials/M_AurelionKit_'+value for key,value in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_Basalt':'PavingBasalt','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal'}.items()}
specs=json.loads((source/'manifest.json').read_text())['modules']
meshes={spec['asset']:import_mesh(spec,source,destination+'/Meshes',materials) for spec in specs}
columns=[('PavingEdge_3x4',-8150),('PavingIvory_4m',-7800),('PavingIvory_4m',-7400),('PavingRoute_4m',-7000),('PavingIvory_4m',-6600),('PavingIvory_4m',-6200),('PavingEdge_3x4',-5850)]
placements=[]
for column,(suffix,x) in enumerate(columns):
    mesh=meshes['SM_Aurelion_KIT_'+suffix]; bounds=mesh.get_bounds(); top=bounds.origin.z+bounds.box_extent.z
    for row,y in enumerate([-17300+400*i for i in range(14)]):
        label=f'KIT_Z01_Paving_{column:02}_{row:02}'
        a=subsystem.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(x,y,-top))
        a.set_actor_label(label); a.set_folder_path('Aurelion/CustomArchitecture/Z01/Paving')
        a.static_mesh_component.set_static_mesh(mesh)
        a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION); a.set_actor_enable_collision(False)
        origin,extent=a.get_actor_bounds(False); assert abs(origin.z+extent.z)<.01
        placements.append(dict(label=label,mesh=mesh.get_path_name(),transform=a.get_actor_transform().export_text(),top_cm=origin.z+extent.z))
for label in ('aureliontarrikfloor','Z01__GoldChannel_01','Z01__GoldChannel_02'):
    component=by_label[label].static_mesh_component
    component.set_visibility(False,False); component.set_hidden_in_game(True,False)
# Retire the old compressed-mesh centre stripe; all 52 instances belong to Z01.
legacy=by_label['Aurelion_Art_M12_Z01_7_e31eb0'].get_component_by_class(unreal.InstancedStaticMeshComponent)
assert legacy and legacy.get_instance_count()==52
assert legacy.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
for i in range(52):
    t=legacy.get_instance_transform(i,world_space=True)
    assert abs(t.translation.x+7000)<.01 and abs(t.translation.y-(-17250+100*i))<.01
legacy.set_visibility(False,False); legacy.set_hidden_in_game(True,False)
# Retire collision on the two replaced decorative strips. Retain the main floor.
expected_state=dict(before)
retired_strips=[]
for label in ('Z01__GoldChannel_01','Z01__GoldChannel_02'):
    actor=by_label[label]; c=actor.static_mesh_component
    assert c.get_collision_enabled()==unreal.CollisionEnabled.QUERY_AND_PHYSICS
    c.modify(); c.set_collision_profile_name('NoCollision')
    c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    key=actor.get_path_name(); transform,enabled,components=before[key]
    assert any(identity==c.get_path_name() for identity,value in components)
    expected_state[key]=(transform,enabled,[(identity,str(unreal.CollisionEnabled.NO_COLLISION) if identity==c.get_path_name() else value) for identity,value in components])
    retired_strips.append(label)
# Check the former strip centers against the retained floor collision.
strip_probes=[]
strip_actors=[by_label[label] for label in retired_strips]
ignore_strips=[a for a in subsystem.get_all_level_actors() if a not in [floor,old_art]+strip_actors]
for actor in strip_actors:
    for y in [-17300+400*i for i in range(14)]:
        x=actor.get_actor_location().x
        raw=unreal.SystemLibrary.line_trace_single(world,unreal.Vector(x,y,100),unreal.Vector(x,y,-50),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,True,ignore_strips,unreal.DrawDebugTrace.NONE,True)
        hit=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
        assert hit.to_tuple()[0] and abs(hit.to_tuple()[5].z)<.2
        strip_probes.append(dict(x=x,y=y,z=hit.to_tuple()[5].z))
(out/'retired-strip-floor-probes.json').write_text(json.dumps(strip_probes,indent=2))
after=snapshot(original); changes={k:dict(before=expected_state[k],after=after[k]) for k in expected_state if expected_state[k]!=after[k]}
(out/'preservation-differences.json').write_text(json.dumps(changes,indent=2)); assert not changes
assert len(placements)==98
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap'); assert editor.save_current_level()
(out/'paving-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',placements=placements,retired_center_guide_instances=52,floor_queries=len(samples),retired_strip_queries=len(strip_probes),intentional_collision_changes=retired_strips,original_actor_count=len(original),qualification='Isolated floor-height queries and authoring checks; live traversal and performance unqualified'),indent=2))
editor.editor_set_game_view(True)
capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
capture=capture.replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('paving-detail',unreal.Vector(-7000,-15100,165),unreal.Rotator(pitch=-45,yaw=90),75)")
exec(compile("p=by_label['Z01_Entry_StandIn']"+capture,'z01_paving_capture','exec'))
