"""Fresh-load end-wall, gate-authority, and previous Z01 assembly verification."""
from pathlib import Path
import runpy
import unreal
root=Path(unreal.Paths.project_dir())
exec(compile((root/'Scripts/Editor/verify_z01_bridge.py').read_text(encoding='utf-8-sig'),'verify_z01_bridge','exec'))
assert len(actors)==1929+z02_paving_count+z02_vault_count+z02_perimeter_count+z02_gallery_count+z02_furniture_count+z02_guard_count+z02_bridges_count+z02_exterior_count and endwall_count==14
for spec in json.loads((root/'Art/Source/Aurelion/EndwallKit/manifest.json').read_text())['modules']:
    mesh=unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Meshes/'+spec['asset'])
    assert sm.get_num_uv_channels(mesh,0)==2 and sm.get_nanite_settings(mesh).get_editor_property('enabled')
    assert sm.get_convex_collision_count(mesh)==0 and sm.get_simple_collision_count(mesh)==0
    assert len(mesh.get_editor_property('static_materials'))==3
checked=[]
for side,y,yaw,gate_label in (('South',-17500,0,'Aurelion_TarrikArrivalGate'),('North',-11900,180,'Aurelion_PressureHallExit')):
    layout=[('Portal',0,'Portal_6x4p5')]+[(f'Bay_{i}',x,'WallPlain_4x7') for i,x in enumerate((-1000,-600,600,1000))]+[(f'Return_{i}',x,'EndReturn_1x7') for i,x in enumerate((-1250,1250))]
    for name,x,suffix in layout:
        label=f'KIT_Z01_Endwall_{side}_{name}'; a=labels[label]; p=a.get_actor_location(); scale=a.get_actor_scale3d()
        assert abs(p.x-(-7000+x))<.01 and abs(p.y-y)<.01 and abs(p.z)<.01
        assert abs(abs(a.get_actor_rotation().yaw)-yaw)<.01
        assert all(abs(v-1)<.001 for v in (scale.x,scale.y,scale.z))
        c=a.static_mesh_component; mesh=c.static_mesh
        assert mesh.get_name()=='SM_Aurelion_KIT_'+suffix
        assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
        assert not a.get_actor_enable_collision() and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
        assert sm.get_num_uv_channels(mesh,0)==2 and sm.get_nanite_settings(mesh).get_editor_property('enabled')
        checked.append(label)
    c=labels[gate_label].visual; t=c.get_world_transform(); p=t.translation; scale=t.scale3d
    assert abs(p.x+7000)<.01 and abs(p.y-y)<.01 and abs(p.z)<.01
    assert all(abs(v-1)<.001 for v in (scale.x,scale.y,scale.z))
    assert abs(abs(c.get_world_rotation().yaw)-yaw)<.01
    assert c.static_mesh.get_name()=='SM_Aurelion_KIT_JournalDoor_6x4p5'
    assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
    assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
    assert len(c.get_editor_property('override_materials'))==0
for label in ('aureliondoors','Z01__UpperSpan_01','Z01__UpperSpan_02'):
    c=labels[label].static_mesh_component
    assert not c.get_editor_property('visible') and c.get_editor_property('hidden_in_game')
    assert c.get_collision_enabled()==unreal.CollisionEnabled.QUERY_AND_PHYSICS
geometry=runpy.run_path(str(root/'Scripts/Editor/check_z01_endwalls.py'))['check_endwalls'](world,actors)
legacy_sign=labels['Aurelion_Art_Sign_Z01_828f4d'].get_component_by_class(unreal.TextRenderComponent)
assert not legacy_sign.get_editor_property('visible') and legacy_sign.get_editor_property('hidden_in_game')
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out/'endwall-reload-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),placements=checked,geometry=geometry,
    qualification='Fresh-load authoring checks only. Live gate state transitions, traversal, combat and performance remain unqualified.'),indent=2))
