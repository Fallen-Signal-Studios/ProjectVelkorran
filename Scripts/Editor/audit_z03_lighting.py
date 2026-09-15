"""Read-only light settings influencing the sensor room."""
from pathlib import Path
import json,os,unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
rows=[]
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    for c in a.get_components_by_class(unreal.LightComponent):
        p=c.get_world_location()
        if not (4500<p.x<9500 and -21500<p.y<-13500):continue
        props={}
        for key in ('intensity','intensity_units','attenuation_radius','cast_shadows','source_width','source_height','indirect_lighting_intensity','volumetric_scattering_intensity'):
            try:props[key]=str(c.get_editor_property(key))
            except Exception:pass
        color=c.get_light_color();rows.append(dict(actor=a.get_actor_label(),path=a.get_path_name(),component=c.get_path_name(),class_name=c.get_class().get_name(),transform=c.get_world_transform().export_text(),location=[p.x,p.y,p.z],color=[color.r,color.g,color.b,color.a],properties=props))
(out/'z03-lights.json').write_text(json.dumps(dict(lights=rows,qualification='Spatial light candidates; rendered comparison required to establish contribution.'),indent=2))
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
