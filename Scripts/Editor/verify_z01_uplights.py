"""Read-only fresh-load checks; does not qualify combat visibility or GPU performance."""
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
exec(compile((root/'Scripts/Editor/verify_z01_vault.py').read_text(encoding='utf-8-sig'),'verify_z01_vault','exec'))
assert len(actors)==1816+paving_count+bridge_count+endwall_count+z02_paving_count+z02_vault_count+z02_perimeter_count+z02_gallery_count+z02_furniture_count+z02_guard_count
checked=[]
for side,face,yaw,direction in (('West',-8245.7608,-90,1),('East',-5774.1984,90,-1)):
    for i,y in enumerate((-16300,-14700,-13100)):
        label='KIT_Z01_Uplight_'+side+'_'+str(i)
        fixture=labels[label]; p=fixture.get_actor_location(); scale=fixture.get_actor_scale3d()
        assert abs(p.x-face)<.01 and abs(p.y-y)<.01 and abs(p.z-630)<.01
        assert all(abs(v-1)<.001 for v in (scale.x,scale.y,scale.z))
        assert abs(fixture.get_actor_rotation().yaw-yaw)<.01
        c=fixture.static_mesh_component; mesh=c.static_mesh
        assert mesh.get_name()=='SM_Aurelion_KIT_PierUplight'
        assert not fixture.get_actor_enable_collision() and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
        assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
        assert sm.get_num_uv_channels(mesh,0)==2 and sm.get_nanite_settings(mesh).get_editor_property('enabled')
        light=labels[label+'_Light']; p=light.get_actor_location()
        assert abs(p.x-(face+direction*27.5))<.01 and abs(p.y-y)<.01 and abs(p.z-679)<.01
        component=light.get_component_by_class(unreal.RectLightComponent)
        assert component.get_editor_property('intensity_units')==unreal.LightUnits.LUMENS
        assert abs(component.get_editor_property('intensity')-1500)<.01
        assert abs(component.get_editor_property('attenuation_radius')-2200)<.01
        assert component.get_editor_property('mobility')==unreal.ComponentMobility.MOVABLE
        checked.append(label)
assert len(checked)==6
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out/'uplight-reload-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),
    fixtures=checked,lights=6,lumens_each=1500,qualification='Authored transforms, settings and visibility only; runtime combat and GPU cost pending'),indent=2))
