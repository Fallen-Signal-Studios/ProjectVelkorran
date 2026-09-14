"""Advance a stopped-editor camera through authored room entry marks.

This only moves the editor camera. It never starts PIE, moves pawns, saves maps,
or awards visual/gameplay acceptance. Re-execute to inspect the next zone.
"""
import unreal

level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor(), 'Art review cannot move a gameplay camera'
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name() in ('L_Aurelion_M12', 'L_Aurelion_M13')
zones = (('Z00', 'Z01', 'Z02', 'Z03', 'Z04', 'Z06', 'Z07', 'Z08', 'Z09')
         if world.get_name() == 'L_Aurelion_M12' else ('Z11', 'Z12'))
state = getattr(unreal, '_aurelion_art_review', (world.get_name(), -1))
index = state[1] + 1 if state[0] == world.get_name() else 0
assert index < len(zones), 'All entry views visited; manually inspect other angles before acceptance'
zone = zones[index]
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
matches = [a for a in actors.get_all_level_actors() if a.get_actor_label() == zone + '_Entry_StandIn']
assert len(matches) == 1
position = matches[0].get_actor_location()
eye = unreal.Vector(position.x, position.y, position.z + 165)
actors.clear_actor_selection_set()
unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(
    eye, unreal.Rotator(pitch=0, yaw=90, roll=0))
unreal._aurelion_art_review = (world.get_name(), index)
unreal.log('AURELION_ART_REVIEW_CAMERA ' + world.get_name() + ' ' + zone + ' ' + eye.export_text())
