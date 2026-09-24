"""Import the owned Z08 pylon pair into an isolated Unreal art-review map."""
import hashlib
import json
import os
from pathlib import Path
import runpy
import time
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
source = root/'Art/Source/Aurelion/Z08ContainmentPylon'
specs = json.loads((source/'manifest.json').read_text())['modules']
assert [row['asset'] for row in specs] == [
    'SM_Aurelion_KIT_Z08ContainmentPylon','SM_Eclipse_KIT_Z08PylonGrowth']
review = '/Game/Aurelion/ArtReview/L_Aurelion_Z08ContainmentPylon'
destination = '/Game/Aurelion/Environment/ArchitectureKit/Meshes'
mapfile = root/'Content/Aurelion/Maps/L_Aurelion_M12.umap'
before = hashlib.sha256(mapfile.read_bytes()).hexdigest()
editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()

materials = {
    'M_Aurelion_PylonBlackStone':'/Game/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_ObservationBlackStone',
    'M_Aurelion_IvoryStone':'/Game/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_Ivory',
    'M_Aurelion_AncientGold':'/Game/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_Gold',
    'M_Aurelion_ChannelShadow':'/Game/Aurelion/Environment/RadianceMaterials/M_Radiance_dark_trim',
    'M_Aurelion_PylonWarmConduit':'/Game/Aurelion/Art/Props/aURELION_pILLAR/Materials/M_GoldEmmissive',
    'M_Eclipse_PylonTissue':'/Game/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_EclipseIntrusion',
    'M_Eclipse_PylonScar':'/Game/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_EclipseIntrusion',
    'M_Eclipse_PylonInternalPulse':'/Game/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_EclipseSeam',
}
helpers = runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
meshes = {}
for spec in specs:
    meshes[spec['asset']] = helpers['import_owned_mesh'](spec,source,destination,materials)

assert not unreal.EditorAssetLibrary.does_asset_exist(review), 'Review map already exists; use a new revision script'
assert editor.new_level(review)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for name in meshes:
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(0,0,0))
    actor.set_actor_label('Z08_REVIEW_'+name)
    actor.static_mesh_component.set_static_mesh(meshes[name])
    # FBX forward is -Y; the Unreal import faces away from the initial camera
    # at yaw zero. Rotate both owned parts together to show their detailed face.
    actor.set_actor_rotation(unreal.Rotator(yaw=180),False)
    if name.startswith('SM_Eclipse_'):
        actor.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)

# Neutral black court and a 180 cm scale figure only in the review map.
floor = actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(0,0,-24))
floor.set_actor_label('Z08_REVIEW_NeutralFloor')
floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
floor.set_actor_scale3d(unreal.Vector(12,10,.48))
floor.static_mesh_component.set_material(0,unreal.load_asset(
    '/Game/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_ObservationBlackStone'))
scale = actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(-430,-190,90))
scale.set_actor_label('Z08_REVIEW_HumanScale_180cm')
scale.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cylinder'))
scale.set_actor_scale3d(unreal.Vector(.24,.24,1.8))
scale.static_mesh_component.set_material(0,unreal.load_asset('/Game/Aurelion/Environment/RadianceMaterials/M_Radiance_dark_trim'))
for position, energy, width in (((-500,-650,850),42000,450),
                                 ((520,-260,620),28000,380),
                                 ((0,400,820),35000,450)):
    location = unreal.Vector(*position)
    rotation = unreal.MathLibrary.find_look_at_rotation(location,unreal.Vector(0,0,400))
    lamp = actors.spawn_actor_from_class(unreal.RectLight,location,rotation)
    component = lamp.get_component_by_class(unreal.RectLightComponent)
    component.set_mobility(unreal.ComponentMobility.MOVABLE)
    component.set_editor_property('intensity_units',unreal.LightUnits.LUMENS)
    component.set_intensity(energy)
    component.set_attenuation_radius(2600)
    component.set_source_width(width)
    component.set_source_height(width)
for index,(fill_position,intensity) in enumerate((((0,-520,530),85000),((420,-220,360),42000))):
    point=actors.spawn_actor_from_class(unreal.PointLight,unreal.Vector(*fill_position))
    point.set_actor_label('Z08_REVIEW_FrontFill_'+str(index))
    fill=point.get_component_by_class(unreal.PointLightComponent)
    fill.set_mobility(unreal.ComponentMobility.MOVABLE)
    fill.set_editor_property('intensity_units',unreal.LightUnits.LUMENS)
    fill.set_intensity(intensity)
    fill.set_attenuation_radius(1800)
post = actors.spawn_actor_from_class(unreal.PostProcessVolume,unreal.Vector())
post.set_editor_property('unbound',True)
settings = post.get_editor_property('settings')
settings.set_editor_property('override_auto_exposure_method',True)
settings.set_editor_property('auto_exposure_method',unreal.AutoExposureMethod.AEM_MANUAL)
settings.set_editor_property('override_auto_exposure_bias',True)
settings.set_editor_property('auto_exposure_bias',1)
post.set_editor_property('settings',settings)

camera = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
position = unreal.Vector(370,-650,500)
rotation = unreal.MathLibrary.find_look_at_rotation(position,unreal.Vector(0,0,390))
camera.set_level_viewport_camera_info(position,rotation)
editor.editor_set_game_view(True)
assert editor.save_current_level()
assert hashlib.sha256(mapfile.read_bytes()).hexdigest() == before
(out/'z08-pylon-import.json').write_text(json.dumps(dict(status='imported_to_review_map',
    map=review,meshes=[dict(path=destination+'/'+s['asset'],triangles=s['triangles'],
                           nominal_dimensions_m=s['nominal_dimensions_m'],
                           convex_hulls=s['convex_hulls']) for s in specs],
    m12_unchanged=True,m12_sha256=before,
    limitations='No campaign placement or player-eye route acceptance'),indent=2))

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state = dict(start=time.monotonic(), capture=False)
def tick(delta):
    elapsed = time.monotonic()-state['start']
    if not state['capture'] and elapsed > 18:
        unreal.AutomationLibrary.take_high_res_screenshot(1200,1200,
            str(out/'z08-pylon-unreal-review.png'))
        state['capture'] = True
    if elapsed > 29:
        unreal.unregister_slate_post_tick_callback(state['handle'])
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
state['handle'] = unreal.register_slate_post_tick_callback(tick)
print('Z08_PYLON_ENGINE_REVIEW_IMPORT_PASS')
