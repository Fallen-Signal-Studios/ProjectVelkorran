"""Read the actual M13 floor assignments and their roughness/metallic inputs."""
import json
from pathlib import Path
import unreal
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Aurelion_M13'
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
rows=[]; materials={}
def node_data(material,node,depth=0):
    if not node: return None
    row=dict(path=node.get_path_name(),type=node.get_class().get_name())
    for name in ('r','default_value','parameter_name','const_a','const_b'):
        try: row[name]=str(node.get_editor_property(name))
        except Exception: pass
    if depth<4:
        row['inputs']=[node_data(material,n,depth+1) for n in unreal.MaterialEditingLibrary.get_inputs_for_material_expression(material,node)]
    return row
for actor in actors:
    for component in actor.get_components_by_class(unreal.StaticMeshComponent):
        p=component.get_world_location()
        assigned=[component.get_material(i) for i in range(component.get_num_materials())]
        if not any(m and any(s in m.get_name().lower() for s in ('floor','lowerblack','blackstone')) for m in assigned): continue
        rows.append(dict(actor=actor.get_actor_label(),component=component.get_name(),position=p.export_text(),
            mesh=component.static_mesh.get_path_name() if component.static_mesh else None,
            materials=[m.get_path_name() if m else None for m in assigned]))
        for material in assigned:
            if not isinstance(material,unreal.Material) or material.get_path_name() in materials: continue
            materials[material.get_path_name()]={name:node_data(material,unreal.MaterialEditingLibrary.get_material_property_input_node(material,prop))
                for name,prop in [('roughness',unreal.MaterialProperty.MP_ROUGHNESS),('metallic',unreal.MaterialProperty.MP_METALLIC),('specular',unreal.MaterialProperty.MP_SPECULAR)]}
out=Path(__file__).resolve().parents[2]/'Saved/Validation/Aurelion/FloorFinish-20260913'
out.mkdir(parents=True,exist_ok=True)
(out/'before.json').write_text(json.dumps(dict(assignments=rows,materials=materials),indent=2),encoding='utf8')
unreal.log('FLOOR_MATERIALS_INSPECTED '+str(out))
