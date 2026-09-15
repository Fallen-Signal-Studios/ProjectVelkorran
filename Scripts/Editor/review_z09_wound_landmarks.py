"""Saved-map visual and component inventory for remaining wound-gallery landmarks."""
from pathlib import Path
import json,os,unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
exec(compile((root/'Scripts/Editor/audit_remaining_environment.py').read_text(),'landmark_census','exec'),globals())
landmarks=[a for a in actors if a.get_actor_label().startswith(('Z09__WoundDiscontinuity','Z09__RestrainedVioletSlit'))]
assert len(landmarks)==6
report=[]
for a in landmarks:
    report.append(dict(actor=a.get_actor_label(),path=a.get_path_name(),class_name=a.get_class().get_name(),tags=[str(t) for t in a.tags],attachment=a.get_attach_parent_actor().get_path_name() if a.get_attach_parent_actor() else None,components=[dict(path=c.get_path_name(),class_name=c.get_class().get_name()) for c in a.get_components_by_class(unreal.ActorComponent)]))
(out/'landmark-components.json').write_text(json.dumps(dict(status='read_only',actors=report,qualification='Component inventory and fixed editor views; no live recognition or witness interaction tested.'),indent=2))
capture=(root/'Scripts/Editor/review_eclipse_wall_scars.py').read_text();prefix,suffix=capture.split('views=[',1)[0],capture.split('state=dict',1)[1]
views="views=[('wound-context',(-200,29900,-1330),(10,90),80),('wound-detail',(-450,30150,-1180),(0,115),65),('violet-context',(-300,27600,-1330),(15,90),80)]\n"
exec(compile(prefix+views+'state=dict'+suffix,'z09_landmark_capture','exec'),globals())
