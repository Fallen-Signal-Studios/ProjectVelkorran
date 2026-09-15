"""Persist required Geometry Collection shader usage on the three owned cargo materials."""
import unreal
for name in ('Reveal', 'PavingIvory', 'Gold'):
    material = unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_' + name)
    assert isinstance(material, unreal.Material)
    material.set_editor_property('used_with_geometry_collections', True)
    unreal.MaterialEditingLibrary.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
unreal.log('CARGO_CHAOS_MATERIAL_USAGE_SAVED')
