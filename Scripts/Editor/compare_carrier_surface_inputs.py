"""Unsaved isolation of carrier distance fields, contact shadows and stone normals."""
from pathlib import Path
import json,unreal
root=Path(unreal.Paths.project_dir())
SKIP_CARRIER_CAPTURE=True
REIMPORT_CARRIER_HULL=False
exec(compile((root/'Scripts/Editor/preview_carrier_refinement.py').read_text(),'carrier_surface_setup','exec'),globals())
visuals=[]
for a in actors:
    if a.get_actor_label() in carrier_labels:
        c=a.get_component_by_class(unreal.InstancedStaticMeshComponent)
        visuals.append((c,c.get_editor_property('affect_distance_field_lighting'),c.get_editor_property('visible')))
        c.set_visibility('_Stable' in a.get_actor_label())
lights=[(c,c.get_editor_property('contact_shadow_length')) for a in actors for c in a.get_components_by_class(unreal.LightComponent)]
stone=unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Materials/M_AurelionKit_PavingIvory')
lib=unreal.MaterialEditingLibrary
normal=lib.get_material_property_input_node(stone,unreal.MaterialProperty.MP_NORMAL)
assert isinstance(normal,unreal.MaterialExpressionLinearInterpolate)
alpha=normal.get_editor_property('const_alpha')
(out/'surface-input-baseline.json').write_text(json.dumps(dict(normal_alpha=alpha,contacts=[dict(component=c.get_path_name(),length=v) for c,v in lights if v],distance_fields=[dict(component=c.get_path_name(),enabled=v) for c,v,visible in visuals]),indent=2))
def surface_mode(name):
    for c,prior,visible in visuals:c.set_editor_property('affect_distance_field_lighting',False if name=='no-distance-field' else prior)
    for c,prior in lights:c.set_editor_property('contact_shadow_length',0 if name=='no-contact' else prior)
    normal.set_editor_property('const_alpha',0 if name=='no-stone-normal' else alpha);lib.recompile_material(stone)
def restore_carrier_surface():
    surface_mode('restore')
    for c,prior,visible in visuals:c.set_visibility(visible)
capture=(root/'Scripts/Editor/review_eclipse_wall_scars.py').read_text()
prefix,suffix=capture.split('views=[',1)[0],capture.split('state=dict',1)[1]
views="views=[(mode,(6500,10000,2000),(-12,-55),75) for mode in ('no-distance-field','no-contact','no-stone-normal')]\n"
suffix=suffix.replace("if state['phase']==0:","if state['phase']==0:\n            surface_mode(name)")
suffix=suffix.replace('restore_capture_settings();editor.eject_pilot_level_actor();','restore_carrier_surface();restore_capture_settings();editor.eject_pilot_level_actor();')
suffix=suffix.replace('except Exception:\n        restore_capture_settings()','except Exception:\n        restore_carrier_surface();restore_capture_settings()')
exec(compile(prefix+views+'state=dict'+suffix,'carrier_surface_capture','exec'),globals())
