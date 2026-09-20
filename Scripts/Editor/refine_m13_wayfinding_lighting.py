"""Preview or save a local light wash on the two dark M13 sign housings."""
from pathlib import Path
import hashlib, json, os, runpy, shutil
import unreal

root = Path(unreal.Paths.project_dir())
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
persist = bool(globals().get('SAVE_WAYFINDING_LIGHTING', False))
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name() == 'L_Aurelion_M13' and not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
helpers = runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
prefix = 'ENVL_M13_WayfindingWash_'
existing = {a.get_actor_label(): a for a in actors.get_all_level_actors()}
before = helpers['snapshot_actor_state']([a for name, a in existing.items() if not name.startswith(prefix)])
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
m12 = root/'Content/Aurelion/Maps/L_Aurelion_M12.umap'
m12_hash = digest(m12)
rows = []
for zone, y in [('Z11', 42000), ('Z12', 46600)]:
    assert 'Aurelion_WayfindingPortal_'+zone in existing
    name = prefix+zone
    location = unreal.Vector(0, y-70, 225)
    rotation = unreal.Rotator(pitch=25, yaw=90)
    light = existing.get(name)
    if light:
        assert isinstance(light, unreal.RectLight)
    else:
        light = actors.spawn_actor_from_class(unreal.RectLight, location, rotation)
    assert light
    light.modify()
    light.set_actor_label(name)
    light.set_folder_path('Aurelion/00_Lighting')
    light.set_actor_location(location, False, False)
    light.set_actor_rotation(rotation, False)
    component = light.get_component_by_class(unreal.RectLightComponent)
    component.set_mobility(unreal.ComponentMobility.MOVABLE)
    component.set_editor_property('intensity_units', unreal.LightUnits.LUMENS)
    component.set_intensity(12)
    component.set_attenuation_radius(480)
    component.set_source_width(340)
    component.set_source_height(8)
    component.set_light_color(unreal.LinearColor(1, .88, .7, 1))
    component.set_editor_property('specular_scale', .15)
    component.set_editor_property('cast_shadows', True)
    rows.append({'actor':name, 'transform':light.get_actor_transform().export_text(),
                 'lumens':12, 'radius_cm':480, 'source_width_cm':340, 'source_height_cm':8})

def verify():
    current = {a.get_actor_label():a for a in actors.get_all_level_actors()}
    assert helpers['snapshot_actor_state']([a for name,a in current.items() if not name.startswith(prefix)]) == before
    assert {name for name in current if name.startswith(prefix)} == {row['actor'] for row in rows}
    for row in rows:
        a = current[row['actor']]
        c = a.get_component_by_class(unreal.RectLightComponent)
        assert a.get_actor_transform().export_text() == row['transform']
        assert abs(c.intensity-row['lumens']) < .01
        assert abs(c.attenuation_radius-row['radius_cm']) < .01
        assert abs(c.source_width-row['source_width_cm']) < .01
        assert abs(c.source_height-row['source_height_cm']) < .01
        assert c.get_editor_property('cast_shadows')
        assert abs(c.get_editor_property('specular_scale')-.15) < .001
    assert digest(m12) == m12_hash

verify()
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M13.umap', out/'L_Aurelion_M13.before.umap')
    assert level.save_current_level()
    assert level.load_level('/Game/Aurelion/Maps/L_Aurelion_M13')
    verify()
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out/'wayfinding-lighting.json').write_text(json.dumps({
    'status':'saved_reloaded' if persist else 'unsaved_preview', 'lights':rows,
    'm12_unchanged':True, 'other_actor_transforms_collision_preserved':True,
    'qualification':'Local architectural lighting; requires visual review. No gameplay or performance qualification.'
}, indent=2))
runpy.run_path(str(root/'Scripts/Editor/preview_m13_route.py'), init_globals={
    'ALLOW_DIRTY_PREVIEW':not persist, 'M13_ROUTE_VIEWS':[
        ('gallery-sign',(0,41500,160)), ('departure-sign',(0,45900,160))]})
