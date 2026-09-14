"""Read-only bounded motion/montage census during a real route; no gameplay requests."""
import json
import os
import time
from pathlib import Path
import unreal

out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])/'eclipse-motion.json'
roles=('Linkbound','WallRunner','Weaver','Elite')
report={'status':'observing','runtime_writes':False,'actors':{},'errors':[],
        'damage_attribution_qualified':False,'route_qualified':False}
state={'start':time.monotonic(),'last':0.,'written':0.,'handle':None,'poses':{},'saw_world':False}

def tick(delta):
    now=time.monotonic()
    if now-state['last']<.25: return
    state['last']=now
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if world:
        state['saw_world']=True
        try:
            for actor in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SovNPCCharacterBase):
                role=next((r for r in roles if actor.get_class().get_name()=='BP_Aurelion'+r+'_C'),None)
                if not role or actor.is_character_pending_load(): continue
                mesh=actor.get_editor_property('mesh'); inst=mesh.get_anim_instance()
                key=actor.get_path_name()
                row=report['actors'].setdefault(key,{'role':role,'samples':0,'moving_samples':0,
                    'moving_pose_changes':0,'maximum_speed':0.,'montages':{},
                    'mesh':mesh.get_skeletal_mesh_asset().get_path_name(),
                    'anim_class':inst.get_class().get_path_name() if inst else None})
                row['samples']+=1
                velocity=actor.get_velocity()
                speed=(velocity.x**2+velocity.y**2+velocity.z**2)**.5
                row['maximum_speed']=max(row['maximum_speed'],speed)
                row['last_position']=actor.get_actor_location().export_text()
                row['last_health']=actor.get_health()
                row['visible']=mesh.is_visible()
                if speed>5. and not actor.get_editor_property('hidden'):
                    row['moving_samples']+=1
                    pose=mesh.get_socket_transform(mesh.get_bone_name(min(12,mesh.get_num_bones()-1)),
                        unreal.RelativeTransformSpace.RTS_COMPONENT).export_text()
                    if key in state['poses'] and state['poses'][key]!=pose: row['moving_pose_changes']+=1
                    state['poses'][key]=pose
                montage=inst.get_current_active_montage() if inst else None
                if montage:
                    name=montage.get_path_name()
                    row['montages'][name]=row['montages'].get(name,0)+1
        except Exception:
            import traceback
            error=traceback.format_exc()
            if error not in report['errors']: report['errors'].append(error)
    report['elapsed_seconds']=now-state['start']
    if (not world and state['saw_world']) or now-state['start']>=3600.:
        unreal.unregister_slate_post_tick_callback(state['handle'])
        report['status']='world_ended' if not world else 'observation_complete'
        out.write_text(json.dumps(report,indent=2),encoding='utf8')
    if now-state['written']>=2.:
        state['written']=now
        out.write_text(json.dumps(report,indent=2),encoding='utf8')

state['handle']=unreal.register_slate_post_tick_callback(tick)
