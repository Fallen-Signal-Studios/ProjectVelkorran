"""Refresh the owned Z08 pylon pair and capture the isolated Unreal review."""
import hashlib
import json
import os
from pathlib import Path
import runpy
import time
import unreal

root=Path(unreal.Paths.project_dir()).resolve()
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
source=root/'Art/Source/Aurelion/Z08ContainmentPylon'
specs=json.loads((source/'manifest.json').read_text())['modules']
assert len(specs)==2
destination='/Game/Aurelion/Environment/ArchitectureKit/Meshes'
mapfile=root/'Content/Aurelion/Maps/L_Aurelion_M12.umap'
before=hashlib.sha256(mapfile.read_bytes()).hexdigest()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert world.get_name()=='L_Aurelion_Z08ContainmentPylon'
assert not editor.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()

materials={
    'M_Aurelion_PylonBlackStone':'/Game/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_ObservationBlackStone',
    'M_Aurelion_IvoryStone':'/Game/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_Ivory',
    'M_Aurelion_AncientGold':'/Game/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_Gold',
    'M_Aurelion_ChannelShadow':'/Game/Aurelion/Environment/RadianceMaterials/M_Radiance_dark_trim',
    'M_Aurelion_PylonWarmConduit':'/Game/Aurelion/Art/Props/aURELION_pILLAR/Materials/M_GoldEmmissive',
    'M_Eclipse_PylonTissue':'/Game/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_EclipseIntrusion',
    'M_Eclipse_PylonScar':'/Game/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_EclipseIntrusion',
    'M_Eclipse_PylonInternalPulse':'/Game/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_EclipseSeam',
}
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
meshes=[helpers['import_owned_mesh'](spec,source,destination,materials) for spec in specs]
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
placed={a.get_actor_label():a for a in actors if a.get_actor_label().startswith('Z08_REVIEW_SM_')}
assert set(placed)=={'Z08_REVIEW_'+spec['asset'] for spec in specs}
for spec,mesh in zip(specs,meshes):
    actor=placed['Z08_REVIEW_'+spec['asset']]
    assert actor.static_mesh_component.static_mesh==mesh
    assert abs(actor.get_actor_rotation().yaw-180)<.1
position=unreal.Vector(370,-650,500)
rotation=unreal.MathLibrary.find_look_at_rotation(position,unreal.Vector(0,0,390))
unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(position,rotation)
editor.editor_set_game_view(True)
assert hashlib.sha256(mapfile.read_bytes()).hexdigest()==before
(out/'z08-pylon-reimport.json').write_text(json.dumps(dict(status='reimported_to_review_map',
    mesh_paths=[m.get_path_name() for m in meshes],
    triangles=[s['triangles'] for s in specs],
    dimensions_m=[s['nominal_dimensions_m'] for s in specs],
    m12_unchanged=True,m12_sha256=before,
    limitations='Art review only; no Z08 campaign placement or traversal proof'),indent=2))
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state=dict(start=time.monotonic(),capture=False)
def tick(delta):
    elapsed=time.monotonic()-state['start']
    if not state['capture'] and elapsed>15:
        unreal.AutomationLibrary.take_high_res_screenshot(1200,1200,
            str(out/'z08-pylon-iteration-unreal.png'))
        state['capture']=True
    if elapsed>25:
        unreal.unregister_slate_post_tick_callback(state['handle'])
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
state['handle']=unreal.register_slate_post_tick_callback(tick)
print('Z08_PYLON_REIMPORT_REVIEW_PASS')
