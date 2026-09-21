"""Earned route to a paused FifthWitness render comparison; not a startup-race regression.

Paces the Selene assent interaction by five seconds, then uses the public cinematic
pause API for rendering diagnostics. No actor poses, materials, lighting or proof
are patched. The known fast-interaction startup race remains unresolved.
"""
import json,os,runpy,sys,time
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
sys.path.insert(0,str(root/'Scripts/Validation/Aurelion'))
import continue_aurelion_m13_input as chamber
original_aim=chamber.Run.aim_use
def paced_aim(self,pc,pawn,actor,kind):
    if self.scene_beat=='SeleneIndependentAssent' and time.monotonic()-self.phase_at<5:
        self.inject();return
    return original_aim(self,pc,pawn,actor,kind)
chamber.Run.aim_use=paced_aim

scope=runpy.run_path(str(root/'Scripts/Validation/Aurelion/review_restored_m13_route.py'))
scope=scope['tick'].__globals__
original_capture=scope['capture'];original_finish=scope['finish']
probe=dict(status='waiting_for_fifth_witness',scope=__doc__,frames=[])
local=dict(active=False,complete=False,index=0)
stages=('lit-before','BaseColor','Roughness','WorldNormal','Metallic','SubsurfaceColor','ShadingModel',
        'lit-after-vt-flush','lit-sss-disabled','lit-restored')
def write(): (out/'fifth-witness-skin.json').write_text(json.dumps(probe,indent=2))
def command(world,value):unreal.SystemLibrary.execute_console_command(world,value)
def restore(world):
    if 'original_flag' not in local:return
    command(world,'ShowFlag.VisualizeBuffer '+str(local['original_flag']))
    command(world,'r.BufferVisualizationTarget '+local['original_target'])
    command(world,'r.SSS.Scale '+str(local['original_sss']))
def finish(error=None):
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if world:restore(world)
    if not local['complete']:probe.update(status='incomplete',route_error=error)
    write();chamber.Run.aim_use=original_aim;original_finish(error)
def setup(world):
    name=stages[local['index']]
    if name.startswith('lit-'):
        restore(world)
        if name=='lit-after-vt-flush':command(world,'r.VT.Flush')
        if name=='lit-sss-disabled':command(world,'r.SSS.Scale 0')
    else:
        command(world,'ShowFlag.VisualizeBuffer 1')
        assert unreal.SystemLibrary.get_console_variable_int_value('ShowFlag.VisualizeBuffer')==1
        command(world,'r.BufferVisualizationTarget '+name)
    local.update(at=time.monotonic(),captured=False)
def capture(world,driver):
    if not local['active']:
        original_capture(world,driver)
        if local['complete'] or scope['state']['phase']!='chamber' or driver.phase!='wait_scene' or driver.scene_beat!='FifthWitness':return
        if time.monotonic()-driver.phase_at<12:return
        if driver.scene_component.get_phase()!=unreal.SovCinematicPhase.PLAYING:return
        assert driver.scene_component.set_cinematic_paused(True),'Native scene pause rejected'
        local.update(active=True,component=driver.scene_component,
            original_flag=unreal.SystemLibrary.get_console_variable_int_value('ShowFlag.VisualizeBuffer'),
            original_target=unreal.SystemLibrary.get_console_variable_string_value('r.BufferVisualizationTarget'),
            original_sss=unreal.SystemLibrary.get_console_variable_float_value('r.SSS.Scale'))
        probe.update(status='capturing',scene=driver.scene.get_path_name(),
            original_sss=local['original_sss'],original_flag=local['original_flag'])
        setup(world);write();return
    assert driver.scene_component==local['component'] and driver.scene_component.get_phase()==unreal.SovCinematicPhase.PAUSED
    age=time.monotonic()-local['at'];name=stages[local['index']]
    if not local['captured'] and age>4:
        camera=unreal.GameplayStatics.get_player_camera_manager(world,0)
        command(world,'Shot -nosuffix filename='+str(out/('skin-'+name+'.png')))
        probe['frames'].append(dict(name=name,camera=camera.get_camera_location().export_text(),
            rotation=camera.get_camera_rotation().export_text()))
        local['captured']=True;write();return
    if local['captured'] and age>6:
        assert (out/('skin-'+name+'.png')).exists()
        local['index']+=1
        if local['index']==len(stages):
            restore(world)
            assert driver.scene_component.set_cinematic_paused(False),'Native scene resume rejected'
            local.update(active=False,complete=True)
            probe.update(status='captured_requires_visual_review',settings_restored=True);write();return
        setup(world)
scope['capture']=capture;scope['finish']=finish
write()
