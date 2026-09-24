"""Read-only M13 side-wall and material audit before a windowed kit preview."""
from pathlib import Path
import hashlib,json,os,runpy
import unreal

root=Path(unreal.Paths.project_dir())
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert world.get_name()=='L_Aurelion_M13' and not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
mapfile=root/'Content/Aurelion/Maps/L_Aurelion_M13.umap'
before=hashlib.sha256(mapfile.read_bytes()).hexdigest()
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
bylabel={a.get_actor_label():a for a in actors}
owner=bylabel['Aurelion_Art_M13_Z12_6_21a580']
side_mesh='/Game/Aurelion/Environment/ArchitectureKit/Meshes/SM_Aurelion_KIT_Z12CofferSide'
side=next(c for c in owner.get_components_by_class(unreal.InstancedStaticMeshComponent)
          if c.static_mesh and c.static_mesh.get_path_name().split('.')[0]==side_mesh)
rows=[]
for i in range(side.get_instance_count()):
    t=side.get_instance_transform(i,world_space=True)
    p=t.translation
    rows.append(dict(index=i,x=round(p.x,3),y=round(p.y,3),z=round(p.z,3),
                     transform=t.export_text()))
assert len(rows)==48
windows=[r for r in rows if abs(r['x'])==2100 and r['y'] in (47300,47700)]
assert len(windows)==16
walls=[]
for label in ('Z12_Wall_EW-1','Z12_Wall_EW1'):
    actor=bylabel[label]
    origin,extent=actor.get_actor_bounds(False)
    component=actor.static_mesh_component
    walls.append(dict(label=label,origin=origin.export_text(),extent=extent.export_text(),
                      component_visible=component.get_editor_property('visible'),
                      collision=str(component.get_collision_enabled()),
                      material=component.get_material(0).get_path_name()))
materials=[]
for path in ('/Game/Aurelion/Environment/RadianceMaterials/M_ScenicPBR_KB3D_UTP_GlassTinted_M13',
             '/Game/Aurelion/Environment/Blender/Shuttles/M_Reformation_Glass',
             '/Game/Aurelion/Environment/Blender/Shuttles/M_Dominion_Glass',
             '/Game/Aurelion/Art/Props/StartingRoomTarrik/Glass'):
    material=unreal.load_asset(path)
    assert material,path
    base=material.get_base_material() if isinstance(material,unreal.MaterialInstance) else material
    materials.append(dict(path=path,cls=material.get_class().get_name(),
                          base=base.get_path_name(),blend=str(base.get_editor_property('blend_mode'))))
central=[]
for actor in actors:
    if actor==owner or actor.get_actor_label() in ('Z12_Wall_EW-1','Z12_Wall_EW1'):
        continue
    origin,extent=actor.get_actor_bounds(False)
    if abs(abs(origin.x)-2100)>200 or abs(origin.y-47500)>240 or origin.z+extent.z<0 or origin.z-extent.z>650:
        continue
    components=[]
    for component in actor.get_components_by_class(unreal.StaticMeshComponent):
        if component.static_mesh:
            components.append(dict(mesh=component.static_mesh.get_path_name(),
                                   visible=component.get_editor_property('visible'),
                                   collision=str(component.get_collision_enabled())))
    if components:central.append(dict(label=actor.get_actor_label(),origin=origin.export_text(),
                                      extent=extent.export_text(),components=components))
central_visual_instances=[]
for actor in actors:
    if not actor.get_actor_label().startswith('Aurelion_Art_M13_Z12_'):
        continue
    for component in actor.get_components_by_class(unreal.InstancedStaticMeshComponent):
        if not component.static_mesh or not component.get_editor_property('visible'):
            continue
        for i in range(component.get_instance_count()):
            t=component.get_instance_transform(i,world_space=True)
            p=t.translation
            if abs(abs(p.x)-2100)<180 and abs(p.y-47500)<120 and -100<p.z<700:
                central_visual_instances.append(dict(actor=actor.get_actor_label(),
                    component=component.get_name(),mesh=component.static_mesh.get_path_name(),
                    index=i,transform=t.export_text(),
                    collision=str(component.get_collision_enabled())))
assert hashlib.sha256(mapfile.read_bytes()).hexdigest()==before
(out/'armored-view-fit-audit.json').write_text(json.dumps(dict(status='read_only',m13_sha256=before,
    side_instances=rows,window_replacement_candidates=windows,native_side_walls=walls,
    glass_candidates=materials,central_obstructions=central,
    central_visual_instances=central_visual_instances),indent=2))
runpy.run_path(str(root/'Scripts/Editor/preview_m13_route.py'),init_globals={
    'M13_ROUTE_VIEWS':[('east-side-before',(0,47500,180)),('west-side-before',(0,47500,180))],
    'M13_ROUTE_YAWS':{'east-side-before':0,'west-side-before':180},
    'M13_ROUTE_PITCHES':{'east-side-before':5,'west-side-before':5}})
print('M13_ARMORED_VIEW_FIT_AUDIT_PASS')
