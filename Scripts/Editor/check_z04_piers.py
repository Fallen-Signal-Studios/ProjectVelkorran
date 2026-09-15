"""Four custom piers retain the original relay-room envelopes at unit scale."""
from pathlib import Path
import json, runpy, unreal

LABEL = 'Aurelion_Art_M12_Z04_18_d57dbf'
MESH = '/Game/Aurelion/Environment/ArchitectureKit/Meshes/SM_Aurelion_KIT_Z08EngagedPier'


def baseline():
    room = json.loads((Path(unreal.Paths.project_dir()) / 'Art/Source/Aurelion/Z04WallKit/room-baseline.json').read_text())
    return next(c for c in room['components'] if c['actor'] == LABEL)


def placements():
    return [((x, y, 0), yaw) for y in (-11700, -10500)
            for x, yaw in ((3975, -90), (10025, 90))]


def check_z04_piers(actors):
    root = Path(unreal.Paths.project_dir())
    old = baseline()
    actor = next(a for a in actors if a.get_actor_label() == LABEL)
    assert actor.get_actor_transform().export_text() == old['actor_transform']
    assert actor.get_actor_enable_collision() == old['actor_collision']
    components = actor.get_components_by_class(unreal.InstancedStaticMeshComponent)
    assert len(components) == 1
    c = components[0]
    assert c.static_mesh == unreal.load_asset(MESH)
    assert c.get_world_transform().export_text() == old['component_transform']
    assert c.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
    assert str(c.get_collision_profile_name()) == 'NoCollision'
    assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
    assert not c.get_editor_property('override_materials')
    sm = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    settings = sm.get_nanite_settings(c.static_mesh)
    assert settings.enabled
    assert sm.get_num_uv_channels(c.static_mesh, 0) == 2
    assert sm.get_simple_collision_count(c.static_mesh) == sm.get_convex_collision_count(c.static_mesh) == 0
    geo = runpy.run_path(str(root / 'Scripts/Editor/check_z08_railings.py'))
    bounds = c.static_mesh.get_bounds()
    origin = [bounds.origin.x, bounds.origin.y, bounds.origin.z]
    extent = [bounds.box_extent.x, bounds.box_extent.y, bounds.box_extent.z]
    assert max(abs(2*extent[i]-[120, 100, 700][i]) for i in range(3)) < .02
    assert c.get_instance_count() == 4
    actual, envelopes = [], []
    for i in range(4):
        t = c.get_instance_transform(i, world_space=True)
        p, r = t.translation, t.rotation.rotator()
        assert (t.scale3d-unreal.Vector(1, 1, 1)).length() < .001
        assert abs(r.pitch)+abs(r.roll) < .001
        actual.append(((round(p.x, 2), round(p.y, 2), round(p.z, 2)), round(r.yaw % 360, 2)))
        envelope = geo['bounds'](geo['corners'](t, origin, extent))
        expected = old['instances'][i]['bounds']
        assert max(abs(v-w) for a, b in zip(envelope, expected) for v, w in zip(a, b)) < .02
        envelopes.append(envelope)
    assert actual == [(p, yaw % 360) for p, yaw in placements()]
    runpy.run_path(str(root / 'Scripts/Editor/check_z04_receivers.py'))['check_z04_receivers'](actors)
    assert len(actors) == 3140
    return dict(piers=4, mesh=MESH, world_envelopes_cm=envelopes,
        retained_nanite_settings=dict(position_precision=settings.position_precision,
            fallback_percent_triangles=settings.fallback_percent_triangles,
            fallback_relative_error=settings.fallback_relative_error),
        qualification='Existing custom Blender mesh reused at unit scale; original visual envelopes and native room collision retained. Structural piers are not campaign destructibles.')
