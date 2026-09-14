"""Add visible physical refuge baffles, preserving existing cast and wall marks.

The openings remain traversable; this is occlusion, not immunity or an AI filter.
Leave unsaved until navigation, mark clearance and viewport review finish.
"""
import unreal

assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
by_label={a.get_actor_label():a for a in subsystem.get_all_level_actors()}
mesh=unreal.load_asset('/Game/Aurelion/Environment/Blender/SM_Aurelion_RecessPanel_2m'); assert mesh
specs=[('West'+str(i),x,1.) for i,x in enumerate((-2900,-2700,-2500,-2300))]
specs += [('East0',2950,.5),('East1',3050,.5),('East2',3150,.5)]
with unreal.ScopedEditorTransaction('Add staggered survivor refuge baffles'):
    for suffix,x,scale in specs:
        label='ART_RefugeBaffle_'+suffix
        a=by_label.get(label)
        if a: assert isinstance(a,unreal.StaticMeshActor) and a.static_mesh_component.static_mesh==mesh
        else: a=subsystem.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(x,22060,-1200))
        a.set_actor_label(label); a.set_folder_path('Aurelion/EnvironmentArt/Z08/RefugeBaffles')
        a.set_actor_location(unreal.Vector(x,22060,-1200),False,False)
        a.set_actor_rotation(unreal.Rotator(yaw=180),False); a.set_actor_scale3d(unreal.Vector(scale,1,1))
        c=a.static_mesh_component; c.set_static_mesh(mesh)
        c.set_collision_profile_name('BlockAll'); c.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
        a.set_actor_enable_collision(True)
    for side,x in (('West',-2600),('East',3050)):
        label='ART_RefugeBaffle_'+side+'Sign'
        sign=by_label.get(label)
        if sign: assert isinstance(sign,unreal.TextRenderActor)
        else: sign=subsystem.spawn_actor_from_class(unreal.TextRenderActor,unreal.Vector(x,22000,-990))
        sign.set_actor_label(label); sign.set_folder_path('Aurelion/EnvironmentArt/Z08/RefugeBaffles')
        sign.set_actor_location(unreal.Vector(x,22000,-990),False,False)
        sign.set_actor_rotation(unreal.Rotator(yaw=-90),False)
        c=sign.get_component_by_class(unreal.TextRenderComponent)
        c.set_text('REFUGE\n<  SIDE ACCESS  >'); c.set_world_size(24)
        c.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
        c.set_text_render_color(unreal.Color(30,24,18,255))
        c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    # Keep the cabinet front clear of the original south refuge wall.
    for label, old_y, new_y in (
        ('Aurelion_RecoveryCacheCabinet_West',22110,22600),
        ('Aurelion_RecoveryCacheCabinet_East',22110,22600),
        ('Aurelion_RecoveryCacheCabinet_Back',22245,22735),
        ('Aurelion_RecoveryCacheCabinet_Roof',22110,22600),
        ('Aurelion_WestMedicalCache',22110,22600)):
        actor=by_label[label]; p=actor.get_actor_location()
        assert abs(p.y-old_y)<.1 or abs(p.y-new_y)<.1, label
        actor.set_actor_location(unreal.Vector(p.x,new_y,p.z),False,False)
    supports=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SovAurelionPrioritySupport)
    assert len(supports)==1
    gate=supports[0].west_cache_barrier
    p=gate.get_world_location(); assert abs(p.y-21985)<.1 or abs(p.y-22475)<.1
    gate.set_world_location(unreal.Vector(p.x,22475,p.z),False,False)
nav=unreal.SovAurelionNavigationLibrary.build_navigation(world,unreal.Vector(0,13000,-500),unreal.Vector(14000,38000,3000))
volume=nav if isinstance(nav,unreal.NavMeshBoundsVolume) else next((v for v in nav if isinstance(v,unreal.NavMeshBoundsVolume)),None) if isinstance(nav,tuple) else None
assert volume is not None, 'Native navigation authoring failed: '+str(nav)
unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(unreal.Vector(-2600,21450,-1030.5),unreal.Rotator(yaw=90))
unreal.log('REFUGE_BAFFLES_PREVIEW_UNSAVED; navigation rebuilding')
