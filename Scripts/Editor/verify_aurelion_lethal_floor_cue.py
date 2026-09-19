"""Read back saved phase cue configuration; this is not a gameplay acceptance test."""
import json
import os
from pathlib import Path
import unreal

tag_name = 'GameplayCue.Aurelion.LethalFloor'
material = unreal.load_asset('/Game/Aurelion/Enemies/Materials/M_AurelionPhaseLattice')
cue = unreal.load_asset('/Game/Cues/Aurelion/GC_AurelionLethalFloor')
assert material and cue
cdo = unreal.get_default_object(cue.generated_class())
assert tag_name in cdo.get_editor_property('gameplay_cue_tag').export_text()
assert cdo.get_editor_property('OverlayMaterial') == material
assert cdo.get_editor_property('Apply to Character?')
assert not cdo.get_editor_property('Apply to Mainhand Weapon?')
assert not cdo.get_editor_property('Apply to Offhand Weapon?')
assert material.get_editor_property('blend_mode') == unreal.BlendMode.BLEND_TRANSLUCENT
assert material.get_editor_property('shading_model') == unreal.MaterialShadingModel.MSM_UNLIT
sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
configured = []
for role in ('Elite', 'Weaver'):
    path = '/Game/Aurelion/Enemies/BP_Aurelion' + role
    bp = unreal.load_asset(path)
    handles = sub.k2_gather_subobject_data_for_blueprint(bp)
    floors = [lib.get_object(lib.get_data(h)) for h in handles
              if isinstance(lib.get_object(lib.get_data(h)), unreal.SovLethalFloorComponent)]
    assert len(floors) == 1, path
    assert tag_name in floors[0].get_editor_property('floor_held_gameplay_cue_tag').export_text(), path
    configured.append(path)
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
result = {'status': 'fresh_load_configuration_pass', 'configured': configured,
          'runtime_verified': False, 'pending': ['visual appearance', 'phase removal', 'health floor and poise in combat']}
(Path(os.environ['SOV_AURELION_RUN_DIRECTORY']) / 'lethal-cue-verification.json').write_text(json.dumps(result, indent=2))
unreal.log('AURELION_LETHAL_CUE_CONFIGURATION_VERIFIED')
