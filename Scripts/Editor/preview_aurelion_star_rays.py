"""Compare the M12 star's additive rays with their supported raster path; no saves."""
from pathlib import Path
import hashlib,json,os,unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
star=next(a for a in actors.get_all_level_actors() if a.get_class().get_name()=='BP_Star1_C')
rays=next(c for c in star.get_components_by_class(unreal.StaticMeshComponent) if c.get_name()=='SM_Rays')
original=rays.get_editor_property('disallow_nanite')
assert not original
bp=unreal.load_asset('/Game/Space_Creator_Pro/Star_Creator/StarCreator_Update_1/Blueprints/BP_Star1')
sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem);lib=unreal.SubobjectDataBlueprintFunctionLibrary
templates=[lib.get_object(lib.get_data(h)) for h in sub.k2_gather_subobject_data_for_blueprint(bp)]
rows=[dict(path=c.get_path_name(),disallow_nanite=c.get_editor_property('disallow_nanite'))
      for c in templates if isinstance(c,unreal.StaticMeshComponent)]
center,extent=star.get_actor_bounds(False)
radius=max(extent.x,extent.y,extent.z)
assert radius>0
position=center+unreal.Vector(0,-radius*3,.15*radius)
rotation=unreal.MathLibrary.find_look_at_rotation(position,center)
maps=[root/'Content/Aurelion/Maps'/n for n in ('L_Aurelion_M12.umap','L_Aurelion_M13.umap')]
hashes={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in maps}
(out/'star-rays-preview.json').write_text(json.dumps(dict(templates=rows,center=center.export_text(),
    extent=extent.export_text(),maps_before=hashes),indent=2))
code=(root/'Scripts/Editor/review_eclipse_wall_scars.py').read_text()
prefix=code.split('views=[',1)[0];suffix=code.split('state=dict',1)[1]
views=[(name,(position.x,position.y,position.z),(rotation.pitch,rotation.yaw),60) for name in ('before','raster-rays')]
suffix=suffix.replace("if state['phase']==0:","if state['phase']==0:\n            rays.set_editor_property('disallow_nanite',name=='raster-rays')")
suffix=suffix.replace('restore_capture_settings();editor.eject_pilot_level_actor();',
    "rays.set_editor_property('disallow_nanite',original);assert all(hashlib.sha256(p.read_bytes()).hexdigest()==hashes[str(p)] for p in maps);restore_capture_settings();editor.eject_pilot_level_actor();")
exec(compile(prefix+'views='+repr(views)+'\nstate=dict'+suffix,'star_rays_capture','exec'),globals())
