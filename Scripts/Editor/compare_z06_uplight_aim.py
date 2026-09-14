"""Unsaved wall-wash aim test at default renderer settings and existing light output."""
from pathlib import Path
import json,os
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
labels={a.get_actor_label():a for a in subsystem.get_all_level_actors()};rows=[]
for side,sign in [('West',-1),('East',1)]:
    for i in range(4):
        light=labels[f'KIT_Z06_Uplight_{side}_{i:02}_Light'];c=light.get_component_by_class(unreal.SpotLightComponent)
        before=light.get_actor_transform().export_text();p=light.get_actor_location()
        rotation=unreal.MathLibrary.find_look_at_rotation(p,unreal.Vector(sign*1600,p.y,50))
        light.set_actor_rotation(rotation,False)
        assert c.get_editor_property('intensity')==100 and c.get_editor_property('cast_shadows')
        rows.append(dict(actor=light.get_actor_label(),before=before,after=light.get_actor_transform().export_text(),lumens=100))
(out/'uplight-aim-comparison.json').write_text(json.dumps(dict(status='unsaved_preview',target_abs_x=1600,lights=rows,qualification='Only eight uplight rotations changed; no light added, no renderer override, no map save.'),indent=2))
exec(compile((root/'Scripts/Editor/preview_z06_gate_housing.py').read_text(),'preview_z06_gate_housing','exec'),globals())
