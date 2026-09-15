"""Read-only refuge sign, native wall and nearby lighting measurements."""
from pathlib import Path
import os,json,unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());rows=[]
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
for a in actors:
    label=a.get_actor_label();p=a.get_actor_location();text=a.get_component_by_class(unreal.TextRenderComponent);light=a.get_component_by_class(unreal.LightComponent)
    if 'SurvivorRecess_' not in label and label not in ('ART_RefugeBaffle_WestSign','ART_RefugeBaffle_EastSign') and not (light and 17500<p.y<24500 and abs(p.x)<7000):continue
    t=a.get_actor_transform();row=dict(actor=label,path=a.get_path_name(),transform=t.export_text(),location=[p.x,p.y,p.z],scale=[t.scale3d.x,t.scale3d.y,t.scale3d.z],quaternion=[t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w],collision=a.get_actor_enable_collision())
    if text:row['text']=dict(value=str(text.text),world_size=text.world_size,horizontal=str(text.horizontal_alignment),vertical=str(text.vertical_alignment),color=text.text_render_color.export_text(),material=text.get_material(0).get_path_name(),transform=text.get_world_transform().export_text())
    if light:row['light']=dict(type=light.get_class().get_name(),intensity=light.intensity,color=light.get_light_color().export_text(),radius=light.attenuation_radius if hasattr(light,'attenuation_radius') else None,units=str(light.intensity_units) if hasattr(light,'intensity_units') else None,shadow=light.cast_shadows,transform=light.get_world_transform().export_text())
    comps=[]
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        m=c.static_mesh;b=m.get_bounds() if m else None
        comps.append(dict(path=c.get_path_name(),mesh=m.get_path_name() if m else None,transform=c.get_world_transform().export_text(),collision=str(c.get_collision_enabled()),profile=str(c.get_collision_profile_name()),origin=[b.origin.x,b.origin.y,b.origin.z] if b else None,extent=[b.box_extent.x,b.box_extent.y,b.box_extent.z] if b else None))
    row['meshes']=comps;rows.append(row)
(out/'refuge-presentation.json').write_text(json.dumps(rows,indent=2))
assert len(actors)==3140 and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
