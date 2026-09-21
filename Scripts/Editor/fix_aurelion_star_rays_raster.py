"""Use raster rendering only for BP_Star1's additive ray component; never save maps."""
from pathlib import Path
import hashlib,json,os,shutil,unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
path='/Game/Space_Creator_Pro/Star_Creator/StarCreator_Update_1/Blueprints/BP_Star1'
disk=root/'Content'/(path.removeprefix('/Game/')+'.uasset')
maps=[root/'Content/Aurelion/Maps'/n for n in ('L_Aurelion_M12.umap','L_Aurelion_M13.umap')]
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
hashes={str(p):digest(p) for p in maps}
bp=unreal.load_asset(path);assert isinstance(bp,unreal.Blueprint)
sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem);lib=unreal.SubobjectDataBlueprintFunctionLibrary
def components():
    return {c.get_name():c for h in sub.k2_gather_subobject_data_for_blueprint(bp)
            for c in [lib.get_object(lib.get_data(h))] if isinstance(c,unreal.StaticMeshComponent)}
def snapshot():
    return {name:dict(mesh=c.static_mesh.get_path_name() if c.static_mesh else None,
        materials=[m.get_path_name() if m else None for m in c.get_materials()],
        location=c.get_editor_property('relative_location').export_text(),
        rotation=c.get_editor_property('relative_rotation').export_text(),
        scale=c.get_editor_property('relative_scale3d').export_text(),
        collision=str(c.get_collision_enabled()),profile=str(c.get_collision_profile_name()),
        shadows=c.get_editor_property('cast_shadow'),disallow_nanite=c.get_editor_property('disallow_nanite'))
        for name,c in components().items()}
before=snapshot();assert 'SM_Rays_GEN_VARIABLE' in before and 'SM_Star_GEN_VARIABLE' in before
rays=components()['SM_Rays_GEN_VARIABLE']
assert rays.get_path_name().startswith(path+'.')
assert not rays.get_editor_property('disallow_nanite'),'Already repaired; use read-only audit'
backup=out/'BP_Star1.before.uasset';assert not backup.exists();shutil.copy2(disk,backup)
rays.modify();rays.set_editor_property('disallow_nanite',True)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
expected=json.loads(json.dumps(before));expected['SM_Rays_GEN_VARIABLE']['disallow_nanite']=True
assert snapshot()==expected,'Unexpected change to another component or visual/collision property'
assert unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
assert all(digest(p)==hashes[str(p)] for p in maps),'A protected map changed on disk'
(out/'star-rays-saved.json').write_text(json.dumps(dict(status='saved_requires_fresh_readback',
    asset=path,backup=str(backup),before=before,after=snapshot(),maps_unchanged=True,
    asset_sha256=digest(disk),qualification='Local marketplace-derived asset; reproducible script is tracked.'),indent=2))
