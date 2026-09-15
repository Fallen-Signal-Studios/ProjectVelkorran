"""Custom carrier art fit with native presentation identity and clearances retained."""
from pathlib import Path
import json, runpy, unreal
DEST='/Game/Aurelion/Environment/ArchitectureKit/Meshes/'
def mesh_name(label):
    return 'SM_Aurelion_KIT_Carrier'+('Nacelle' if '_Engine' in label else 'Bridge' if label.endswith('_Forward') else 'Hull')

def art_transform(label,by_label):
    phase='Stable' if '_Stable' in label else 'Suspended'
    hull=by_label['Aurelion_Carrier_'+phase].get_actor_transform()
    hull.scale3d=unreal.Vector(1,1,1)
    if '_Engine' in label:
        local=unreal.Transform(location=unreal.Vector(-2600 if label.endswith('Engine-1') else 2600,500,250))
    elif label.endswith('_Forward'):
        local=unreal.Transform(location=unreal.Vector(0,5300,250),rotation=unreal.Rotator(pitch=-15))
    else:return hull
    return unreal.MathLibrary.compose_transforms(local,hull)

def check_carrier_kit(actors):
    root=Path(unreal.Paths.project_dir())
    baseline=json.loads((root/'Art/Source/Aurelion/CarrierKit/carrier-baseline.json').read_text())
    by_label={a.get_actor_label():a for a in actors}
    geo=runpy.run_path(str(root/'Scripts/Editor/check_z08_railings.py'))
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    rows=[]
    for old in baseline['parts']:
        a=by_label[old['label']]
        assert a.get_path_name()==old['path'] and a.get_actor_transform().export_text()==old['transform']
        assert (a.get_attach_parent_actor().get_path_name() if a.get_attach_parent_actor() else None)==old['parent']
        assert a.get_actor_enable_collision()==old['collision'] and a.get_editor_property('hidden')==old['hidden']
        cs=a.get_components_by_class(unreal.StaticMeshComponent)
        assert len(cs)==2
        for oldc in old['components']:
            c=next(c for c in cs if c.get_path_name()==oldc['path'])
            assert c.get_world_transform().export_text()==oldc['transform']
            assert c.get_editor_property('visible')==oldc['visible'] and c.get_editor_property('hidden_in_game')==oldc['hidden']
            assert not c.get_editor_property('can_ever_affect_navigation')
            assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and str(c.get_collision_profile_name())=='NoCollision'
            if not isinstance(c,unreal.InstancedStaticMeshComponent):
                assert c.static_mesh.get_path_name()==oldc['mesh']
                continue
            mesh=c.static_mesh
            assert mesh==unreal.load_asset(DEST+mesh_name(old['label']))
            assert c.get_instance_count()==1 and not c.get_editor_property('override_materials')
            t=c.get_instance_transform(0,world_space=True)
            expected=art_transform(old['label'],by_label)
            assert (t.translation-expected.translation).length()<.01
            assert (t.scale3d-unreal.Vector(1,1,1)).length()<.001
            assert t.rotation.angular_distance(expected.rotation)<.0001
            b=mesh.get_bounds();o=[b.origin.x,b.origin.y,b.origin.z];e=[b.box_extent.x,b.box_extent.y,b.box_extent.z]
            envelope=geo['bounds'](geo['corners'](t,o,e))
            assert envelope[0][0]>=7799.9, (old['label'],envelope)
            # Preserve each original part's nominal size at its corrected banked
            # pose. Comparing to the unbanked world box would reject that repair.
            scale=a.get_actor_scale3d()
            prior=geo['bounds'](geo['corners'](expected,[0,0,0],[scale.x*50,scale.y*50,scale.z*50]))
            assert all(envelope[0][i]>=prior[0][i]-30 and envelope[1][i]<=prior[1][i]+30 for i in range(3)), (old['label'],envelope,prior)
            ns=sm.get_nanite_settings(mesh)
            assert ns.enabled and ns.position_precision==10 and ns.fallback_percent_triangles==1
            assert sm.get_num_uv_channels(mesh,0)==2 and sm.get_simple_collision_count(mesh)==sm.get_convex_collision_count(mesh)==0
            rows.append(dict(label=old['label'],mesh=mesh.get_path_name(),envelope_cm=envelope,
                corrected_nominal_envelope_cm=prior,old_unbanked_envelope_cm=old['components'][0]['bounds'],old_tiles=len(oldc['instances'])))
    legacy=json.loads((root/'Art/Source/Aurelion/CarrierKit/legacy-spire-baseline.json').read_text())
    a=by_label[legacy['actor']];c=a.static_mesh_component
    assert a.get_actor_transform().export_text()==legacy['actor_transform'] and c.static_mesh.get_path_name()==legacy['mesh']
    assert not c.get_editor_property('visible') and c.get_editor_property('hidden_in_game')
    assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
    assert len(rows)==8 and len(actors)==baseline['actor_count']==3140
    return dict(parts=rows,removed_tiles=sum(r['old_tiles'] for r in rows),qualification='Static art fit and preserved native ownership; live rescue transition and performance remain separate qualification.')
