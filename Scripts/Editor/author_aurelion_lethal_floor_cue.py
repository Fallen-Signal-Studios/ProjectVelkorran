"""Author a phase-protection lattice and configure the Elite/Weaver floor components."""
import json
import os
import shutil
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assets = unreal.EditorAssetLibrary
edit = unreal.MaterialEditingLibrary
tag = unreal.GameplayTag()
assert tag.import_text('(TagName="GameplayCue.Aurelion.LethalFloor")')
assert 'TagName="GameplayCue.Aurelion.LethalFloor"' in tag.export_text()
material_path = '/Game/Aurelion/Enemies/Materials/M_AurelionPhaseLattice'
cue_path = '/Game/Cues/Aurelion/GC_AurelionLethalFloor'
assert not assets.does_asset_exist(material_path), 'Do not overwrite authored material'
assert not assets.does_asset_exist(cue_path), 'Do not overwrite authored cue'
mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    'M_AurelionPhaseLattice', '/Game/Aurelion/Enemies/Materials', unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property('two_sided', True)
mat.set_editor_property('used_with_skeletal_mesh', True)
shape = edit.create_material_expression(mat, unreal.MaterialExpressionCustom, -200, 0)
shape.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT4)
inputs = []
for key in ('Position', 'Seconds', 'Color'):
    pin = unreal.CustomInput()
    pin.set_editor_property('input_name', key)
    inputs.append(pin)
shape.set_editor_property('inputs', inputs)
shape.set_editor_property('code', '''
float bandDistance = abs(frac(Position.z / 32.0) - 0.5);
float line = 1.0 - smoothstep(0.025, 0.065, bandDistance);
float rung = 1.0 - smoothstep(0.025, 0.06, abs(frac((Position.x + Position.y) / 48.0) - 0.5));
float pulse = 0.65 + 0.35 * sin(Seconds * 3.14159265);
float lattice = saturate(line + rung * 0.32);
return float4(Color.rgb * (1.2 + pulse) * lattice, lattice * (0.35 + pulse * 0.3));
''')
position = edit.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -600, -150)
seconds = edit.create_material_expression(mat, unreal.MaterialExpressionTime, -600, 0)
color = edit.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -600, 150)
color.set_editor_property('parameter_name', 'Color')
color.set_editor_property('default_value', unreal.LinearColor(0.42, 0.12, 1.0, 1.0))
for source, key in ((position, 'Position'), (seconds, 'Seconds'), (color, 'Color')):
    assert edit.connect_material_expressions(source, '', shape, key)
for channels, prop, y in (((True, True, True, False), unreal.MaterialProperty.MP_EMISSIVE_COLOR, 0),
                          ((False, False, False, True), unreal.MaterialProperty.MP_OPACITY, 200)):
    mask = edit.create_material_expression(mat, unreal.MaterialExpressionComponentMask, 100, y)
    for channel, enabled in zip(('r', 'g', 'b', 'a'), channels): mask.set_editor_property(channel, enabled)
    assert edit.connect_material_expressions(shape, '', mask, '')
    assert edit.connect_material_property(mask, '', prop)
edit.recompile_material(mat)
assert assets.save_loaded_asset(mat, only_if_is_dirty=False)

# Copy the inspected implementation, including WhileActive mesh application and OnRemove cleanup.
cue = assets.duplicate_asset('/Game/Cues/OverlayEffect/GC_OverlayEffect', cue_path)
assert cue
cdo = unreal.get_default_object(cue.generated_class())
cdo.set_editor_property('gameplay_cue_tag', tag)
cdo.set_editor_property('OverlayMaterial', mat)
cdo.set_editor_property('OverlayColor', unreal.LinearColor(0.42, 0.12, 1.0, 1.0))
cdo.set_editor_property('Apply to Character?', True)
cdo.set_editor_property('Apply to Mainhand Weapon?', False)
cdo.set_editor_property('Apply to Offhand Weapon?', False)
unreal.BlueprintEditorLibrary.compile_blueprint(cue)
assert assets.save_loaded_asset(cue, only_if_is_dirty=False)

sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
configured = []
for role in ('Elite', 'Weaver'):
    path = '/Game/Aurelion/Enemies/BP_Aurelion' + role
    bp = unreal.load_asset(path)
    shutil.copy2(Path(unreal.Paths.project_dir()) / ('Content/Aurelion/Enemies/BP_Aurelion'+role+'.uasset'), out/('BP_Aurelion'+role+'.before.uasset'))
    handles = list(sub.k2_gather_subobject_data_for_blueprint(bp))
    roots = [h for h in handles if lib.is_root_actor(lib.get_data(h))]
    floors = [lib.get_object(lib.get_data(h)) for h in handles if isinstance(lib.get_object(lib.get_data(h)), unreal.SovLethalFloorComponent)]
    assert len(roots) == 1 and len(floors) <= 1
    if not floors:
        handle, reason = sub.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=roots[0], new_class=unreal.SovLethalFloorComponent, blueprint_context=bp))
        assert lib.is_handle_valid(handle), str(reason)
        component = lib.get_object(lib.get_data(handle))
    else:
        component = floors[0]
        assert component.get_path_name().startswith(path), 'Do not mutate inherited components'
    component.set_editor_property('floor_held_gameplay_cue_tag', tag)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert assets.save_loaded_asset(bp, only_if_is_dirty=False)
    configured.append(path)
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages(), 'Do not save maps during this asset pass'
(out/'lethal-cue-authoring.json').write_text(json.dumps({'status':'saved_requires_runtime_validation','cue':cue_path,'material':material_path,'configured':configured}, indent=2))
unreal.log('AURELION_LETHAL_CUE_SAVED')
