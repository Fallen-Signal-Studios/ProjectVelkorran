"""Receiver presentation, native interaction ownership and remaining device poses."""
from pathlib import Path
import json,runpy,unreal
def check_z04_receivers(actors):
    root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/Z04ReceiverKit';old=json.loads((source/'devices-baseline.json').read_text());labels={a.get_actor_label():a for a in actors}
    for row in old['devices']:
        a=labels[row['actor']];assert a.get_path_name()==row['path'] and a.get_actor_transform().export_text()==row['transform']
        if row['actor'].startswith('Aurelion_Art'):
            original=next(c for c in row['components'] if 'instances' in c);c=a.get_component_by_class(unreal.InstancedStaticMeshComponent)
            assert c.static_mesh.get_path_name()==original['mesh'] and c.get_instance_count()==1
            assert c.get_instance_transform(0,world_space=True).export_text()==original['instances'][2]
            assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
            continue
        for r in row['components']:
            c=next(c for c in a.get_components_by_class(unreal.ActorComponent) if c.get_name()==r['name'])
            if row['actor'].startswith('Aurelion_E2_Receiver') and r['name']=='Visual':continue
            if isinstance(c,unreal.SceneComponent):
                assert c.get_world_transform().export_text()==r['transform']
                assert (c.get_attach_parent().get_path_name() if c.get_attach_parent() else None)==r['parent']
            if isinstance(c,unreal.PrimitiveComponent):assert str(c.get_collision_enabled())==r['collision'] and str(c.get_collision_profile_name())==r['profile']
            if isinstance(c,unreal.StaticMeshComponent):assert (c.static_mesh.get_path_name() if c.static_mesh else None)==r['mesh']
        if not row['actor'].startswith('Aurelion_E2_Receiver'):continue
        assert str(a.receiver_id)==row['properties']['receiver_id'] and a.encounter_objective.get_path_name()==row['properties']['encounter_objective'].split("'")[1]
        assert abs(a.interactable.interaction_distance-250)<.01 and abs(a.interactable.interaction_time-.5)<.001
        assert (a.body.get_unscaled_box_extent()-unreal.Vector(30,45,65)).length()<.001
        c=a.visual;m=c.static_mesh
        assert m.get_name()=='SM_Aurelion_KIT_Z04Receiver' and c.get_attach_parent()==a.body
        assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and str(c.get_collision_profile_name())=='NoCollision'
        assert not c.get_editor_property('override_materials') and not c.get_editor_property('hidden_in_game') and c.get_editor_property('visible')
        t=c.get_relative_transform();r=t.rotation.rotator()
        assert (t.translation-unreal.Vector(0,0,-66)).length()<.001 and (t.scale3d-unreal.Vector(1,1,1)).length()<.001 and abs(r.yaw+90)<.001 and abs(r.pitch)+abs(r.roll)<.001
        b=m.get_bounds();assert (b.box_extent-unreal.Vector(45,30,65.5)).length()<.02 and (b.origin-unreal.Vector(0,0,65.5)).length()<.02
        sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);n=sm.get_nanite_settings(m)
        assert n.enabled and n.position_precision==10 and n.fallback_percent_triangles==1 and n.fallback_relative_error==0 and sm.get_num_uv_channels(m,0)==2
        assert sm.get_simple_collision_count(m)==sm.get_convex_collision_count(m)==0
    material=unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_ReceiverStatus');lib=unreal.MaterialEditingLibrary
    blend=lib.get_material_property_input_node(material,unreal.MaterialProperty.MP_EMISSIVE_COLOR);assert isinstance(blend,unreal.MaterialExpressionLinearInterpolate)
    active,inactive,flag=lib.get_inputs_for_material_expression(material,blend)
    assert isinstance(flag,unreal.MaterialExpressionScalarParameter) and str(flag.get_editor_property('parameter_name'))=='ReceiverDisabled' and flag.get_editor_property('use_custom_primitive_data') and flag.get_editor_property('primitive_data_index')==0
    assert active.get_editor_property('constant').g>1 and inactive.get_editor_property('constant').g<.01
    for name in ('Aurelion_E2_ReceiverEast','Aurelion_E2_ReceiverWest'):assert material in labels[name].visual.get_materials()
    runpy.run_path(str(root/'Scripts/Editor/check_z04_wayfinding.py'))['check_z04_wayfinding'](actors)
    return dict(receivers=2,retired_duplicate_prop_instances=4,status_primitive_data_index=0,interaction_distance_cm=250,interaction_seconds=.5,qualification='Authored mesh, native interaction bindings and material graph only; actual disable/retry/reload and live readability remain unqualified.')
