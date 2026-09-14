"""Position only the ejected editor camera for inspection; never move a gameplay actor."""
import sys
import unreal
role=sys.argv[1] if len(sys.argv)>1 else 'WallRunner'
assert role in ('WallRunner','Weaver','Elite','Linkbound')
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world
actors=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SovNPCCharacterBase)
        if a.get_class().get_name()=='BP_Aurelion'+role+'_C' and not a.get_editor_property('hidden')]
assert actors
target=actors[0].get_actor_location()
eye=target+unreal.Vector(320.,-320.,115.)
rotation=unreal.MathLibrary.find_look_at_rotation(eye,target)
unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(eye,rotation)
unreal.log('ECLIPSE_REVIEW_CAMERA '+role+' '+actors[0].get_path_name())
