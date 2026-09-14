"""Fresh static route bindings, repaired coverage and preceding architecture checks."""
from pathlib import Path
import runpy,time
import unreal
root=Path(unreal.Paths.project_dir());DEFER_Z08_WALLS_AUTORUN=True
exec(compile((root/'Scripts/Editor/verify_z08_walls.py').read_text(),'verify_z08_walls','exec'),globals())
verify_z08_walls_stage=verify
del DEFER_Z08_WALLS_AUTORUN
def verify():
    verify_z08_walls_stage();bindings=[]
    for group,name in [('E3','SM_Aurelion_KIT_Z06ClimbPanel'),('E4','SM_Aurelion_KIT_Z08WallAssembly')]:
        routes=[a for a in actors if isinstance(a,unreal.SovAurelionWallRoute) and str(a.get_editor_property('route_id'))=='Aurelion.'+group+'.WallEntry'];assert len(routes)==1
        route=routes[0];surfaces=route.get_editor_property('presentation_surfaces');assert len(surfaces)==1
        c=surfaces[0];assert c.static_mesh.get_name()==name and c.get_editor_property('visible') and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
        assert not route.get_components_by_class(unreal.PrimitiveComponent),'Cosmetic contact must not add serialized physics components'
        bindings.append(dict(route=route.get_actor_label(),surface=c.get_path_name(),mesh=name))
    m=unreal.load_asset('/Game/Aurelion/Environment/ArchitectureKit/Meshes/SM_Aurelion_KIT_Z08WallAssembly');settings=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).get_nanite_settings(m)
    assert settings.get_editor_property('fallback_percent_triangles')==1 and settings.get_editor_property('fallback_relative_error')==0
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    (out/'wall-contact-bindings-verification.json').write_text(json.dumps(dict(status='passed',actor_count=len(actors),bindings=bindings,qualification='Saved bindings and architecture; separate native tests cover query behavior. Live gait, camera and performance remain unqualified.'),indent=2))
if not globals().get('DEFER_WALL_CONTACT_AUTORUN',False):
    unreal.EditorPythonScripting.set_keep_python_script_alive(True);started=time.monotonic()
    def tick(delta):
        if time.monotonic()-started<15:return
        unreal.unregister_slate_post_tick_callback(handle)
        try:verify()
        finally:unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    handle=unreal.register_slate_post_tick_callback(tick)
