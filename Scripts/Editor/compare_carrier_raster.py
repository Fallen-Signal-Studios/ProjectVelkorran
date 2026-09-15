"""Compare the same refined scene using Nanite versus its full fallback mesh."""
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
SKIP_CARRIER_CAPTURE=True
REIMPORT_CARRIER_HULL=False
exec(compile((root/'Scripts/Editor/preview_carrier_refinement.py').read_text(),'carrier_raster_setup','exec'),globals())
visuals=[]
for a in actors:
    if a.get_actor_label() in carrier_labels:
        c=a.get_component_by_class(unreal.InstancedStaticMeshComponent)
        visuals.append((c,c.get_editor_property('disallow_nanite')))
        c.set_visibility('_Stable' in a.get_actor_label())
capture=(root/'Scripts/Editor/review_eclipse_wall_scars.py').read_text()
prefix,suffix=capture.split('views=[',1)[0],capture.split('state=dict',1)[1]
views="views=[(mode,(6500,10000,2000),(-12,-55),75) for mode in ('nanite','full-raster')]\n"
suffix=suffix.replace("if state['phase']==0:","if state['phase']==0:\n            for c,prior in visuals: c.set_editor_property('disallow_nanite',name=='full-raster')")
suffix=suffix.replace('restore_capture_settings();editor.eject_pilot_level_actor();',"for c,prior in visuals:\n                c.set_editor_property('disallow_nanite',prior);c.set_visibility(True)\n            restore_capture_settings();editor.eject_pilot_level_actor();")
exec(compile(prefix+views+'state=dict'+suffix,'carrier_raster_capture','exec'),globals())
