"""Verify visual cladding placement without changing mission collision."""
import json
import os
from pathlib import Path
import unreal

assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name() == 'L_Aurelion_M12'
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
panels = sorted((a for a in actors if a.get_actor_label().startswith('ART_RecessPanel_')), key=lambda a:a.get_actor_label())
assert len(panels) == 10
placements = [('West', x, 1.) for x in (-3100,-2900,-2700,-2500,-2300,-2100)]
placements += [('East', x, scale) for x,scale in ((2800,1.),(3000,1.),(3200,1.),(3350,.5))]
expected = {'ART_RecessPanel_' + side + '_' + str(i+1).zfill(2): (x, scale)
            for i, (side, x, scale) in enumerate(placements)}
rows = []
for actor in panels:
    x, scale = expected[actor.get_actor_label()]
    c = actor.get_component_by_class(unreal.StaticMeshComponent)
    p = actor.get_actor_location()
    assert not actor.get_actor_enable_collision()
    assert c.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
    mesh_name=c.get_editor_property('static_mesh').get_name()
    if mesh_name!='SM_Aurelion_RecessPanel_2m':
        assert mesh_name=='SM_Aurelion_KIT_RefugePanel_'+str(1 if scale==.5 else 2)+'m'
        scale=1.
    assert abs(p.y-22870) < .1 and abs(p.z+1200) < .1
    assert abs(p.x-x) < .1
    actual_scale = actor.get_actor_scale3d()
    assert abs(actual_scale.x-scale) < .001 and abs(actual_scale.y-1) < .001 and abs(actual_scale.z-1) < .001
    assert abs(abs(actor.get_actor_rotation().yaw)-180) < .1
    rows.append(dict(label=actor.get_actor_label(), position=p.export_text(), scale=actor.get_actor_scale3d().export_text()))
report = dict(status='PASS', panels=rows,
    scope='10 noncolliding visual cladding actors; gameplay passages and survivor protection are not qualified')
for side in ('West', 'East'):
    lights = [a for a in actors if a.get_actor_label() == 'ENVL_Z08_RecessFill_' + side]
    assert len(lights) == 1
    c = lights[0].get_component_by_class(unreal.RectLightComponent)
    assert c and abs(c.get_editor_property('intensity')-350) < .1
    assert c.get_editor_property('intensity_units') == unreal.LightUnits.LUMENS
    assert abs(c.get_editor_property('attenuation_radius')-1400) < .1
report['local_fill_lights'] = dict(count=2, lumens_each=350, attenuation_cm=1400)
baseline = json.loads((Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])/'environment-cast-audit.json').read_text())
by_label = {a.get_actor_label(): a for a in actors}
checked = 0
for row in baseline['maps']['L_Aurelion_M12']['actors']:
    if not row['label'].startswith('Z') or not any(c['collision'] != '<CollisionEnabled.NO_COLLISION: 0>' for c in row['components']):
        continue
    actor = by_label[row['label']]
    origin, extent = actor.get_actor_bounds(False)
    assert origin.export_text() == row['bounds_origin'] and extent.export_text() == row['bounds_extent'], row['label']
    assert actor.get_actor_location().export_text() == row['location'], row['label']
    checked += 1
report['unchanged_zone_collision_actor_bounds'] = checked
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])/'recess-panel-placement.json'
out.write_text(json.dumps(report, indent=2), encoding='utf8')
unreal.log('RECESS_PANEL_PLACEMENT_VERIFIED 10')
