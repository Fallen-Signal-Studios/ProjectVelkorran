"""Read back custom seats and existing walls after public earned CP9 loading."""
from pathlib import Path
import hashlib,json,runpy
import unreal
root=Path(unreal.Paths.project_dir())
saved=json.loads((root/'Saved/Validation/Aurelion/DepartureSeatSaved-20260920-233612-ab3b2dfb/departure-seat-fit.json').read_text())
assert saved['status']=='saved_reloaded'
asset=root/'Content/Aurelion/Environment/ArchitectureKit/Meshes/SM_Aurelion_KIT_DepartureSeat.uasset'
assert hashlib.sha256(asset.read_bytes()).hexdigest()==saved['mesh_sha256']
driver=runpy.run_path(str(Path(__file__).with_name('verify_m13_departure_walls.py')))
driver=driver['tick'].__globals__;parent=driver['scope'];previous_finish=driver['finish']
driver['views']=[('player',None,None),('seat-east',(1500,46800,150),45),
    ('seat-west',(-1500,47400,150),-135),('selene-background',(1350,48200,170),-90)]

def finish(error=None):
    if error:previous_finish(error);return
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    actors=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.StaticMeshActor)
    actual={}
    for label,row in saved['expected'].items():
        actor=next(a for a in actors if a.get_actor_label()==label)
        c=actor.get_component_by_class(unreal.StaticMeshComponent)
        assert not actor.get_actor_enable_collision() and not c.get_editor_property('can_ever_affect_navigation')
        assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
        actual[label]=dict(transform=actor.get_actor_transform().export_text(),mesh=c.static_mesh.get_path_name(),collision=str(c.get_collision_enabled()))
    assert actual==saved['expected']
    parent['report']['departure_seat_review']=dict(saved_seats=actual,qualification='Earned load and exact runtime asset/pose readback; frame review remains separate.')
    previous_finish()
parent['finish']=finish
