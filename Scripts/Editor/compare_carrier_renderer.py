"""Use renderer-wide Nanite switching to validate the component-only comparison."""
from pathlib import Path
import json,unreal
root=Path(unreal.Paths.project_dir());SKIP_CARRIER_CAPTURE=True;REIMPORT_CARRIER_HULL=False
exec(compile((root/'Scripts/Editor/preview_carrier_refinement.py').read_text(),'carrier_renderer_setup','exec'),globals())
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
visuals=[]
for a in actors:
    if a.get_actor_label() in carrier_labels:
        c=a.get_component_by_class(unreal.InstancedStaticMeshComponent)
        visuals.append((c,c.get_editor_property('visible')));c.set_visibility('_Stable' in a.get_actor_label())
fills=[(c,c.source_width,c.source_height) for c in owner.get_components_by_class(unreal.RectLightComponent)]
for c,w,h in fills:c.set_source_width(w*.1);c.set_source_height(h*.1)
before=unreal.SystemLibrary.get_console_variable_int_value('r.Nanite')
assert before==1
def renderer_mode(name):
    unreal.SystemLibrary.execute_console_command(world,'r.Nanite '+str(0 if name=='raster' else before))
    assert unreal.SystemLibrary.get_console_variable_int_value('r.Nanite')==(0 if name=='raster' else before)
def restore_renderer():
    renderer_mode('restore')
    for c,v in visuals:c.set_visibility(v)
    for c,w,h in fills:c.set_source_width(w);c.set_source_height(h)
capture=(root/'Scripts/Editor/review_eclipse_wall_scars.py').read_text()
prefix,suffix=capture.split('views=[',1)[0],capture.split('state=dict',1)[1]
views="views=[(mode,(6500,10000,2000),(-12,-55),75) for mode in ('small-emitter','raster')]\n"
suffix=suffix.replace("if state['phase']==0:","if state['phase']==0:\n            renderer_mode(name)")
suffix=suffix.replace('restore_capture_settings();editor.eject_pilot_level_actor();','restore_renderer();restore_capture_settings();editor.eject_pilot_level_actor();')
suffix=suffix.replace('except Exception:\n        restore_capture_settings()','except Exception:\n        restore_renderer();restore_capture_settings()')
exec(compile(prefix+views+'state=dict'+suffix,'carrier_renderer_capture','exec'),globals())
