"""Fresh-load paving checks and isolated floor/retired-strip height queries."""
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
exec(compile((root/'Scripts/Editor/verify_z01_uplights.py').read_text(encoding='utf-8-sig'),'verify_z01_uplights','exec'))
assert len(actors)==1914+bridge_count+endwall_count+z02_paving_count+z02_vault_count+z02_perimeter_count+z02_gallery_count+z02_furniture_count+z02_guard_count+z02_bridges_count+z02_exterior_count+z02_facade_light_count+north_guard_count+north_bridge_count+atrium_guard_count+atrium_ring_count+atrium_parapet_count+atrium_canopy_count
legacy=labels['Aurelion_Art_M12_Z01_7_e31eb0'].get_component_by_class(unreal.InstancedStaticMeshComponent)
assert legacy.get_instance_count()==52
assert not legacy.get_editor_property('visible') and legacy.get_editor_property('hidden_in_game')
assert legacy.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
columns=[('PavingEdge_3x4',-8150),('PavingIvory_4m',-7800),('PavingIvory_4m',-7400),('PavingRoute_4m',-7000),('PavingIvory_4m',-6600),('PavingIvory_4m',-6200),('PavingEdge_3x4',-5850)]
checked=[]
for col,(suffix,x) in enumerate(columns):
    for row in range(14):
        label=f'KIT_Z01_Paving_{col:02}_{row:02}'; a=labels[label]; p=a.get_actor_location()
        assert abs(p.x-x)<.01 and abs(p.y-(-17300+row*400))<.01
        scale=a.get_actor_scale3d(); assert all(abs(v-1)<.001 for v in (scale.x,scale.y,scale.z))
        assert abs(a.get_actor_rotation().yaw)<.01
        origin,extent=a.get_actor_bounds(False); assert abs(origin.z+extent.z)<.01
        c=a.static_mesh_component; mesh=c.static_mesh
        assert mesh.get_name()=='SM_Aurelion_KIT_'+suffix
        assert not a.get_actor_enable_collision() and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
        assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
        assert sm.get_num_uv_channels(mesh,0)==2 and sm.get_nanite_settings(mesh).get_editor_property('enabled')
        checked.append(label)
for label in ('aureliontarrikfloor','Z01__GoldChannel_01','Z01__GoldChannel_02'):
    c=labels[label].static_mesh_component
    assert not c.get_editor_property('visible') and c.get_editor_property('hidden_in_game')
    expected=unreal.CollisionEnabled.QUERY_AND_PHYSICS if label=='aureliontarrikfloor' else unreal.CollisionEnabled.NO_COLLISION
    assert c.get_collision_enabled()==expected,(label,str(c.get_collision_enabled()),str(c.get_collision_profile_name()))
    if label!='aureliontarrikfloor': assert str(c.get_collision_profile_name())=='NoCollision'
floor=labels['Z01_Floor']; assert floor.static_mesh_component.get_collision_enabled()==unreal.CollisionEnabled.QUERY_AND_PHYSICS
targets=[floor,labels['aureliontarrikfloor'],labels['Z01__GoldChannel_01'],labels['Z01__GoldChannel_02']]
ignored=[a for a in actors if a not in targets]; samples=[]
for x in (-8250,-8150,-7800,-7400,-7180,-7000,-6820,-6600,-6200,-5850,-5750):
    for y in [-17300+400*i for i in range(14)]:
        raw=unreal.SystemLibrary.line_trace_single(world,unreal.Vector(x,y,100),unreal.Vector(x,y,-50),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,True,ignored,unreal.DrawDebugTrace.NONE,True)
        hit=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
        assert hit.to_tuple()[0] and abs(hit.to_tuple()[5].z)<.2
        samples.append(dict(x=x,y=y,z=hit.to_tuple()[5].z))
assert len(checked)==98 and len(samples)==154
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out/'paving-reload-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),
    placements=checked,floor_height_queries=samples,qualification='Saved geometry and isolated floor collision; live traversal, combat and GPU performance unqualified'),indent=2))
