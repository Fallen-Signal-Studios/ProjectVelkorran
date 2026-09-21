"""Preview the exact particle-reader repair; never save the asset or map."""
from pathlib import Path
import json,os,time,traceback,unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
path='/NarrativePro/Pro/Core/VFX/Lyra/Effects/Particles/Weapons/NS_WeaponFire_Tracer_Reformation'
asset=unreal.load_asset(path)
assert asset and unreal.SovCombatFeedbackAuthoringLibrary.finish_feedback_compilation(asset)
base=asset.get_path_name()+':'
readers=[unreal.load_object(None,base+suffix) for suffix in (
    'SystemUpdateScript.NiagaraDataInterfaceParticleRead_3',
    'Sparks2_1.NiagaraScriptSource_0.NiagaraGraph_0.NiagaraNodeInput_1.SpawnParticlesFromOtherEmitter_Attribute_Reader')]
assert all(readers)
expected=globals().get('EXPECTED_READER','ref')
assert all(str(r.get_editor_property('EmitterBinding').get_editor_property('EmitterName'))==expected for r in readers)
info=unreal.SovCombatFeedbackAuthoringLibrary.inspect_feedback_system(asset)
assert [str(e.name) for e in info.emitters]==['Tracer','Sparks1','Sparks2','Sparks3','Flash']
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
back=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(-150,0,200),unreal.Rotator(pitch=-90))
back.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Plane'))
back.set_actor_scale3d(unreal.Vector(10,15,1))
light=actors.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector(500,0,500),unreal.Rotator(pitch=-35,yaw=160))
light.get_component_by_class(unreal.DirectionalLightComponent).set_intensity(3)
camera=actors.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(1100,0,200),unreal.Rotator(yaw=180))
camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(60)
level.pilot_level_actor(camera);level.editor_set_game_view(True);level.editor_set_viewport_realtime(True)
effect=actors.spawn_actor_from_class(unreal.NiagaraActor,unreal.Vector(0,-400,200))
component=effect.get_component_by_class(unreal.NiagaraComponent)
component.set_asset(asset);component.set_force_solo(True)
component.set_niagara_variable_position('User.MuzzlePosition',unreal.Vector(0,-400,200))
unreal.NiagaraDataInterfaceArrayFunctionLibrary.set_niagara_array_vector(component,'User.ImpactPositions',[unreal.Vector(0,400,200)])
component.set_niagara_variable_bool('User.Trigger',True)
report=dict(status='preparing',assets_saved=[],maps_saved=[],readers=[r.get_path_name() for r in readers],captures=[])
state=dict(stage=0,next=time.monotonic()+20,task=None,busy=False,start=time.monotonic())
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
def finish():
    (out/'tracer-preview.json').write_text(json.dumps(report,indent=2))
    unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False)
def tick(dt):
    if state['busy'] or time.monotonic()<state['next']:return
    state['busy']=True
    try:
        assert time.monotonic()-state['start']<160
        if state['task']:
            if not state['task'].is_task_done():return
            assert Path(report['captures'][-1]).exists()
            state['task']=None;state['stage']+=1
            if state['stage']==2:
                report.update(status='captured_requires_visual_review',bindings=[r.get_editor_property('EmitterBinding').export_text() for r in readers])
                finish();return
            component.deactivate()
            for reader in readers:
                binding=reader.get_editor_property('EmitterBinding')
                binding.set_editor_property('EmitterName','Tracer')
                reader.set_editor_property('EmitterBinding',binding)
            assert unreal.SovCombatFeedbackAuthoringLibrary.finish_feedback_compilation(asset)
        component.set_paused(False);component.reinitialize_system()
        unreal.log('TRACER_RENDER_STAGE_'+str(state['stage'])+'_EXPECTED_'+expected)
        component.advance_simulation(8,1/60);component.set_paused(True)
        filename=out/('tracer-before.png' if state['stage']==0 else 'tracer-after.png')
        state['task']=unreal.AutomationLibrary.take_high_res_screenshot(1800,1000,str(filename),camera)
        report['captures'].append(str(filename));state['next']=time.monotonic()+3
    except Exception:
        report.update(status='failed',error=traceback.format_exc());finish()
    finally:state['busy']=False
handle=unreal.register_slate_post_tick_callback(tick)
