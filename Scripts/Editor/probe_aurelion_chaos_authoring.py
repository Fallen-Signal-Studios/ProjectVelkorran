"""Transient by default; optional persistence is restricted to isolated review assets."""
from pathlib import Path
import json, os, unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
required = ['Dataflow', 'DataflowEditorBlueprintLibrary', 'DataflowBlueprintLibrary',
            'GeometryCollection', 'GeometryCollectionComponent']
available = {name: hasattr(unreal, name) for name in required}
(out/'chaos-capabilities.json').write_text(json.dumps(available, indent=2))
assert all(available.values()), available
graph = unreal.new_object(unreal.Dataflow)
collection = unreal.new_object(unreal.GeometryCollection)
lib = unreal.DataflowEditorBlueprintLibrary

def node(kind, name, x):
    result = lib.add_dataflow_node(graph, kind, name, unreal.Vector2D(x, 0))
    assert str(result) != 'None', kind
    return result

source = node('FStaticMeshToCollectionDataflowNode_v2', 'CargoSource', 0)
fracture = node('FUniformFractureDataflowNode', 'CargoFracture', 350)
terminal = node('FGeometryCollectionTerminalDataflowNode_v2', 'CargoTerminal', 700)
mesh = unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Meshes/SM_Aurelion_KIT_Z08SealedStores')
assert mesh
assert lib.set_dataflow_node_property(graph, source, 'StaticMesh', mesh.get_path_name())
assert lib.set_dataflow_node_property(graph, source, 'bSplitComponents', 'False')
for key, value in dict(MinVoronoiSites=12, MaxVoronoiSites=12, RandomSeed=914,
                       SplitIslands=False, Grout=0, Amplitude=0).items():
    assert lib.set_dataflow_node_property(graph, fracture, key, str(value)), key
for a, output, b, input in [(source,'Collection',fracture,'Collection'),
                             (fracture,'Collection',terminal,'Collection'),
                             (source,'Materials',terminal,'Materials')]:
    assert lib.connect_dataflow_nodes(graph, a, output, b, input)
unreal.DataflowBlueprintLibrary.evaluate_terminal_node_by_name(graph, terminal, collection)
component = unreal.new_object(unreal.GeometryCollectionComponent)
component.set_rest_collection(collection)
transforms = component.get_initial_local_rest_transforms()
result = dict(status='transient_probe', transforms=len(transforms),
              debug=component.get_debug_info(), root_index=component.get_root_index(),
              bounds=str(component.get_local_bounds()),
              materials=[m.get_path_name() if m else None for m in collection.get_editor_property('materials')],
              qualification='Authoring experiment only; no live physics or combat qualification.')
(out/'chaos-authoring-probe.json').write_text(json.dumps(result, indent=2))
assert len(transforms)>1, result
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
unreal.log('AURELION_CHAOS_TRANSIENT_AUTHORING_PASS')

if globals().get('SAVE_CHAOS_PROTOTYPE', False):
    destination='/Game/Aurelion/ArtReview/Chaos'
    tools=unreal.AssetToolsHelpers.get_asset_tools()
    for name in ['DF_Aurelion_CargoPrototype', 'GC_Aurelion_CargoPrototype']:
        assert not unreal.EditorAssetLibrary.does_asset_exist(destination+'/'+name), name
    saved_graph=tools.duplicate_asset('DF_Aurelion_CargoPrototype',destination,graph)
    saved_collection=tools.duplicate_asset('GC_Aurelion_CargoPrototype',destination,collection)
    assert saved_graph and saved_collection
    saved_collection.set_dataflow_asset(saved_graph)
    instance=saved_collection.get_editor_property('dataflow_instance')
    instance.set_editor_property('dataflow_terminal',terminal)
    saved_collection.set_editor_property('dataflow_instance',instance)
    saved_collection.set_editor_property('enable_clustering',True)
    saved_collection.set_editor_property('damage_threshold',[500000.0])
    saved_collection.set_editor_property('mass_as_density',False)
    saved_collection.set_editor_property('mass',120.0)
    for asset in (saved_graph,saved_collection):
        assert unreal.EditorAssetLibrary.save_loaded_asset(asset)
    (out/'chaos-prototype-assets.json').write_text(json.dumps(dict(
        graph=saved_graph.get_path_name(),collection=saved_collection.get_path_name(),
        status='saved_prototype',qualification='Not placed in campaign; live physics validation pending.'),indent=2))
