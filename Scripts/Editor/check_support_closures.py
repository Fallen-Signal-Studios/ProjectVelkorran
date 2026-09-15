"""Authored roof collision and native-scaled priority gate presentation."""
from pathlib import Path
import json,unreal

def check_support_closures(actors):
    root=Path(unreal.Paths.project_dir());labels={a.get_actor_label():a for a in actors};sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    baseline=json.loads((root/'Art/Source/Aurelion/CacheEnclosureKit/native-baseline.json').read_text())['components']
    roof=labels['Aurelion_RecoveryCacheCabinet_Roof'];c=roof.static_mesh_component;old=next(r for r in baseline if r['actor']==roof.get_actor_label())
    assert roof.get_path_name()==old['path'] and c.get_path_name()==old['component']
    assert (roof.get_actor_location()-unreal.Vector(-3050,22600,-910)).length()<.001 and roof.get_actor_scale3d()==unreal.Vector(1,1,1)
    r=roof.get_actor_rotation();assert abs(r.pitch)+abs(r.yaw)+abs(r.roll)<.001
    assert c.static_mesh.get_name()=='SM_Aurelion_KIT_CacheRoof' and c.get_collision_enabled()==unreal.CollisionEnabled.QUERY_AND_PHYSICS and str(c.get_collision_profile_name())=='BlockAll'
    assert roof.get_actor_enable_collision()==old['actor_collision'] and sm.get_convex_collision_count(c.static_mesh)==1
    contacts=[]
    for x in (-3150,-3050,-2950):
        for y in (22500,22600,22700):
            for sign in (-1,1):
                hit=c.line_trace_component(unreal.Vector(x,y,-910+sign*30),unreal.Vector(x,y,-910),False,False,False)
                assert hit is not None and abs(hit[0].z-(-910+sign*10))<.01
                contacts.append(hit[0].export_text())
    for x,y in ((-3191,22600),(-2909,22600),(-3050,22454),(-3050,22746)):
        assert c.line_trace_component(unreal.Vector(x,y,-950),unreal.Vector(x,y,-870),False,False,False) is None
    presentation=labels['Aurelion_SupportBarrierView'];support=labels['Aurelion_PrioritySupport']
    assert presentation.support==support and labels['Aurelion_WestMedicalCache'].support==support
    gates=[]
    for side,visual,barrier,size in [('West',presentation.west_barrier_visual,support.west_cache_barrier,(2.4,.4,2.8)),('East',presentation.east_barrier_visual,support.east_flank_barrier,(9,.4,4.5))]:
        expected='SM_Aurelion_KIT_'+('WestCacheGate' if side=='West' else 'EastFlankGate')
        assert visual.static_mesh.get_name()==expected and visual.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
        assert str(visual.get_collision_profile_name())=='NoCollision' and sm.get_convex_collision_count(visual.static_mesh)==0
        assert (visual.get_world_location()-barrier.get_world_location()).length()<.001
        assert (visual.get_world_scale()-unreal.Vector(*size)).length()<.001
        assert visual.get_world_rotation()==barrier.get_world_rotation()
        b=visual.static_mesh.get_bounds();assert b.origin.length()<.001 and (b.box_extent-unreal.Vector(50,50,50)).length()<.01
        old=next(r for r in baseline if r['component']==barrier.get_path_name())
        assert barrier.get_world_transform().export_text()==old['transform'] and str(barrier.get_collision_enabled())==old['collision']
        assert [barrier.get_unscaled_box_extent().x,barrier.get_unscaled_box_extent().y,barrier.get_unscaled_box_extent().z]==old['box_extent']
        gates.append(dict(side=side,mesh=expected,transform=visual.get_world_transform().export_text(),native_barrier_preserved=True))
    for component in (c,presentation.west_barrier_visual,presentation.east_barrier_visual):
        assert component.get_editor_property('visible') and not component.get_editor_property('hidden_in_game') and not component.get_owner().get_editor_property('hidden') and not component.get_editor_property('override_materials')
        mesh=component.static_mesh;n=sm.get_nanite_settings(mesh)
        assert n.enabled and n.position_precision==10 and n.fallback_percent_triangles==1 and n.fallback_relative_error==0
        assert sm.get_num_uv_channels(mesh,0)==2 and sm.get_simple_collision_count(mesh)==0
    assert len(actors)==3140
    return dict(roof_contacts=contacts,roof_outside_misses=4,gates=gates,qualification='Saved presentation and exact roof solid; runtime priority opening and save/reload remain unqualified.')
