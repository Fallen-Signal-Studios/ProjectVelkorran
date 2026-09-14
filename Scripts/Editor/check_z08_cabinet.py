"""True-size medical sideboard placement and physical clearance contract."""
from pathlib import Path
import json,runpy,unreal

def check_z08_cabinet(actors):
    root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/Z08CabinetKit'
    old=json.loads((source/'cabinet-baseline.json').read_text());a=next(a for a in actors if a.get_actor_label()==old['actor']);c=a.get_component_by_class(unreal.InstancedStaticMeshComponent);mesh=c.static_mesh
    assert a.get_actor_transform().export_text()==old['actor_transform'] and c.get_world_transform().export_text()==old['component_transform'] and c.get_path_name()==old['component']
    assert a.get_actor_enable_collision() and c.get_collision_enabled()==unreal.CollisionEnabled.QUERY_AND_PHYSICS and str(c.get_collision_profile_name())=='BlockAll'
    assert mesh.get_name()=='SM_Aurelion_KIT_Z08MedicalSideboard' and c.get_instance_count()==1
    assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game') and not c.get_editor_property('override_materials')
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);n=sm.get_nanite_settings(mesh)
    assert sm.get_num_uv_channels(mesh,0)==2 and n.enabled and n.position_precision==10 and n.fallback_percent_triangles==1 and n.fallback_relative_error==0
    assert sm.get_convex_collision_count(mesh)==0 and sm.get_simple_collision_count(mesh)==1
    assert any(mesh.get_material(i).get_name()=='M_AurelionKit_MedicalUpholstery' for i in range(len(mesh.get_editor_property('static_materials'))))
    mat=unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_MedicalUpholstery');lib=unreal.MaterialEditingLibrary
    assert mat.get_editor_property('used_with_nanite') and mat.get_editor_property('used_with_instanced_static_meshes')
    assert abs(lib.get_material_property_input_node(mat,unreal.MaterialProperty.MP_ROUGHNESS).get_editor_property('r')-.82)<1e-5
    assert lib.get_material_property_input_node(mat,unreal.MaterialProperty.MP_METALLIC).get_editor_property('r')==0
    color=lib.get_material_property_input_node(mat,unreal.MaterialProperty.MP_BASE_COLOR).get_editor_property('constant')
    assert max(abs(v-w) for v,w in zip((color.r,color.g,color.b),(.035,.075,.072)))<1e-5
    helper=runpy.run_path(str(root/'Scripts/Editor/check_z08_railings.py'));b=mesh.get_bounds();origin=[b.origin.x,b.origin.y,b.origin.z];extent=[b.box_extent.x,b.box_extent.y,b.box_extent.z];rows=[]
    for row in old['instances']:
        t=c.get_instance_transform(row['index'],world_space=True);placement=json.loads((source/'placement.json').read_text());prior=unreal.Transform(location=unreal.Vector(*placement['location_cm']),rotation=unreal.Rotator(yaw=placement['yaw_degrees']))
        assert (t.scale3d-unreal.Vector(1,1,1)).length()<1e-5
        assert abs(abs(sum(getattr(t.rotation,k)*getattr(prior.rotation,k) for k in ('x','y','z','w')))-1)<1e-6
        actual=helper['bounds'](helper['corners'](t,origin,extent));target=placement['bounds_cm'];assert (t.translation-prior.translation).length()<.01
        error=max(abs(v-w) for av,ev in zip(actual,target) for v,w in zip(av,ev));assert error<.02,(row['index'],error,actual,target)
        rows.append(dict(index=row['index'],bounds_error_cm=error,transform=t.export_text()))
    assert len(actors)==3140
    return dict(instances=rows,qualification='Relocated medical sideboard with bounds-box collision; live evacuation and performance remain unqualified.')


def check_cabinet_access(actors):
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert not unreal.SovAurelionNavigationLibrary.is_navigation_being_built_or_locked(world)
    def hit(raw):
        if raw is None:return dict(blocked=False,actor=None)
        values=[v for v in raw if isinstance(v,unreal.HitResult)] if isinstance(raw,tuple) else [raw]
        assert len(values)==1 and isinstance(values[0],unreal.HitResult)
        p=values[0].to_tuple()
        return dict(blocked=bool(p[0]),actor=p[9].get_actor_label() if p[9] else None,impact_z=p[5].z)
    cabinet=next(a for a in actors if a.get_actor_label()=='Aurelion_Art_M12_Z08_49_b7d737')
    floor=[]
    for x,y in [(-2645,22745),(-2555,22745),(-2645,22815),(-2555,22815)]:
        row=hit(unreal.SystemLibrary.line_trace_single(world,unreal.Vector(x,y,-1190),unreal.Vector(x,y,-1220),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[cabinet],unreal.DrawDebugTrace.NONE,True))
        assert row['blocked'] and abs(row['impact_z']+1200)<2,row
        floor.append(row)
    blocking=hit(unreal.SystemLibrary.line_trace_single(world,unreal.Vector(-2600,22650,-1160),unreal.Vector(-2600,22780,-1160),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[],unreal.DrawDebugTrace.NONE,True))
    assert blocking['blocked'] and blocking['actor']==cabinet.get_actor_label(),blocking
    paths=[]
    for name,end in [('refuge',(-2750,22150,-1110)),('cache',(-3050,22380,-1110)),('sideboard',(-2600,22650,-1110))]:
        nav=unreal.SovAurelionNavigationLibrary.find_path_to_location_synchronously(world,unreal.Vector(-2600,21750,-1110),unreal.Vector(*end),None,None)
        row=dict(name=name,complete=bool(nav and nav.is_valid() and not nav.is_partial()),points=[p.export_text() for p in nav.path_points] if nav else [])
        assert row['complete'],row
        endpoint=nav.path_points[-1];row['endpoint_error_xy_cm']=((endpoint.x-end[0])**2+(endpoint.y-end[1])**2)**.5
        assert row['endpoint_error_xy_cm']<50,row
        paths.append(row)
    return dict(floor_contacts=floor,blocking_contact=blocking,paths=paths,qualification='Stopped-editor navigation and physical contacts; live evacuation remains unqualified.')
