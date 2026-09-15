"""Isolate carrier self-shadows and local fill shadows; never save the map."""
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
SKIP_CARRIER_CAPTURE=True
REIMPORT_CARRIER_HULL=False
exec(compile((root/'Scripts/Editor/preview_carrier_refinement.py').read_text(),'carrier_shadow_setup','exec'),globals())
visuals=[]
for a in actors:
    if a.get_actor_label() in carrier_labels:
        c=a.get_component_by_class(unreal.InstancedStaticMeshComponent)
        visuals.append((c,c.get_editor_property('cast_shadow'),c.get_editor_property('visible')))
        c.set_visibility('_Stable' in a.get_actor_label())
fills=[(c,c.get_editor_property('cast_shadows')) for c in owner.get_components_by_class(unreal.LightComponent)]
def shadow_mode(name):
    for c,prior,visible in visuals:c.set_cast_shadow(False if name=='no-carrier-shadows' else prior)
    for c,prior in fills:c.set_cast_shadows(False if name=='no-fill-shadows' else prior)
def restore_carrier_shadows():
    for c,prior,visible in visuals:c.set_cast_shadow(prior);c.set_visibility(visible)
    for c,prior in fills:c.set_cast_shadows(prior)
capture=(root/'Scripts/Editor/review_eclipse_wall_scars.py').read_text()
prefix,suffix=capture.split('views=[',1)[0],capture.split('state=dict',1)[1]
views="views=[(mode,(6500,10000,2000),(-12,-55),75) for mode in ('baseline','no-fill-shadows','no-carrier-shadows')]\n"
suffix=suffix.replace("if state['phase']==0:","if state['phase']==0:\n            shadow_mode(name)")
suffix=suffix.replace('restore_capture_settings();editor.eject_pilot_level_actor();','restore_carrier_shadows();restore_capture_settings();editor.eject_pilot_level_actor();')
suffix=suffix.replace('except Exception:\n        restore_capture_settings()','except Exception:\n        restore_carrier_shadows();restore_capture_settings()')
exec(compile(prefix+views+'state=dict'+suffix,'carrier_shadow_capture','exec'),globals())
