from pathlib import Path
import unreal
PERSIST=True
exec(compile((Path(unreal.Paths.project_dir())/'Scripts/Editor/preview_z06_walls.py').read_text(),'save_z06_walls','exec'))
# Record local-light settings for the following room lighting pass, without changing them.
light_rows=[]
for actor in original:
    for light in actor.get_components_by_class(unreal.LightComponent):
        if not isinstance(light,(unreal.PointLightComponent,unreal.RectLightComponent)):continue
        p=light.get_world_transform().translation
        if not (-2200<=p.x<=2200 and 5800<=p.y<=11800 and -1100<=p.z<=1500):continue
        light_rows.append(dict(actor=actor.get_actor_label(),component=light.get_name(),class_name=light.get_class().get_name(),transform=light.get_world_transform().export_text(),intensity=light.get_editor_property('intensity'),units=str(light.get_editor_property('intensity_units')),radius=light.get_editor_property('attenuation_radius')))
(out/'z06-local-lights.json').write_text(json.dumps(dict(status='read_only_inventory',lights=light_rows),indent=2))
