"""Read-only upper-vault placement and preserved-geometry verification."""
import json
from pathlib import Path
import unreal

root=Path(__file__).resolve().parents[2]
out=root/'Saved/Validation/Aurelion/CrucibleVault-20260913'
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Aurelion_M12'
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
by_name={a.get_name():a for a in actors}; by_label={a.get_actor_label():a for a in actors}
baseline=json.loads((out/'physical-baseline.json').read_text())
procedural_drift=[]
component_transform_drift=[]
for name,row in baseline.items():
    a=by_name[name]
    assert a.get_actor_label()==row['label'] and a.get_actor_transform().export_text()==row['transform'],name
    components={c.get_name():c for c in a.get_components_by_class(unreal.PrimitiveComponent)}
    assert set(components)==set(row['components']),name
    for cname,before in row['components'].items():
        c=components[cname]
        if name=='Ultra_Dynamic_Sky_C_0' and cname in ('BillboardComponent_5','BillboardComponent_6','BillboardComponent_7'):
            assert isinstance(c,unreal.BillboardComponent) and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
        else:
            current_transform=c.get_world_transform().export_text()
            if current_transform!=before['transform']:
                component_transform_drift.append(dict(actor=name,component=cname,before=before['transform'],after=current_transform,collision=before['collision']))
        assert str(c.get_collision_enabled())==before['collision'],(name,cname)
        if before['instances'] is not None:
            current=[c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]
            if current!=before['instances']:
                # Observed after reload: six unrelated asteroid fields rebuild
                # their 84 instances per component. Keep this as a failed world
                # invariant, not an ignored or accepted geometry difference.
                assert name in {'BP_AsteroidField_Globular_C_'+str(i) for i in range(1,7)} and cname in {'ISM_Asteroid_'+str(i) for i in range(1,5)},(name,cname)
                procedural_drift.append(dict(actor=name,component=cname,before_count=len(before['instances']),after_count=len(current)))
ceiling=by_label['Aurelion_Art_M12_Z08_89_b5e7cb'].get_component_by_class(unreal.InstancedStaticMeshComponent)
assert ceiling.get_instance_count()==216 and not ceiling.get_editor_property('visible') and ceiling.get_editor_property('hidden_in_game')
vault=by_label['ART_Crucible_UpperVault']; p=vault.get_actor_location()
assert p==unreal.Vector(0,20800,-500) and vault.get_actor_scale3d()==unreal.Vector(1,1,1)
assert vault.get_actor_rotation()==unreal.Rotator()
c=vault.static_mesh_component
assert c.static_mesh.get_name()=='SM_Aurelion_CrucibleVault'
assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and not vault.get_actor_enable_collision()
assert c.get_num_materials()==4 and all(c.get_material(i) for i in range(4))
origin,extent=vault.get_actor_bounds(False)
assert abs(origin.z-extent.z+500)<.2 and abs(origin.z+extent.z-1040)<.2
for side in (-1,1):
    for index,y in enumerate((19600,22000)):
        light=by_label['ENVL_CrucibleVault_'+str(side)+'_'+str(index)]
        assert light.get_actor_location()==unreal.Vector(side*2300,y,300)
        assert abs(light.get_actor_rotation().pitch-18)<.1
        c=light.get_component_by_class(unreal.RectLightComponent)
        assert c.get_editor_property('intensity_units')==unreal.LightUnits.LUMENS
        for key,value in (('intensity',2500),('attenuation_radius',3000),('source_width',1400),('source_height',800)):
            assert abs(c.get_editor_property(key)-value)<.1
old=by_label['Aurelion_Art_Sign_Z08_1cd400'].get_component_by_class(unreal.TextRenderComponent)
assert not old.get_editor_property('visible') and old.get_editor_property('hidden_in_game')
sign=by_label['ART_Crucible_WoundGallerySign']
assert sign.get_actor_location()==unreal.Vector(0,23160,-590)
c=sign.get_component_by_class(unreal.TextRenderComponent)
assert str(c.text)=='WOUND GALLERY' and c.world_size==45
report=dict(vault_status='PASS',world_instance_invariance='FAIL_PROCEDURAL_ENVIRONMENT_DRIFT' if procedural_drift or component_transform_drift else 'PASS',original_actor_transforms_and_collision_checked=len(baseline),procedural_drift=procedural_drift,component_transform_drift=component_transform_drift,vault_collision='none',old_ceiling_instances_hidden=216,new_material_slots=4,
    scope='Editor placement and saved-content checks only; combat, cinematics and packaged GPU performance remain pending')
(out/'verified.json').write_text(json.dumps(report,indent=2),encoding='utf8')
unreal.log('CRUCIBLE_VAULT_VERIFIED; WORLD_INSTANCE_INVARIANCE='+report['world_instance_invariance'])
