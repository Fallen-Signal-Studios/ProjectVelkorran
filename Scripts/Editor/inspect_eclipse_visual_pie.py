"""Observe real PIE appearance/pose/readiness without moving or activating actors."""
import json
import time
from pathlib import Path
import unreal

OUT=Path(unreal.Paths.project_dir()).resolve()/'Saved/Validation/Aurelion/EclipseAnimation'
OUT.mkdir(parents=True,exist_ok=True)
REPORT={'status':'observing','runtime_writes':False,'samples':[],'errors':[],
        'combat_qualified':False,'locomotion_qualified':False}
STATE={'started':time.monotonic(),'last':0.,'handle':None}

def path(obj): return obj.get_path_name() if obj else None

def tick(delta):
    now=time.monotonic()
    if now-STATE['last']<1.: return
    STATE['last']=now
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if world:
        try:
            row={'elapsed':now-STATE['started'],'enemies':[]}
            player=unreal.GameplayStatics.get_player_pawn(world,0)
            if player:
                visual=player.get_character_visual()
                row['player']={'pending_load':player.is_character_pending_load(),
                               'visual':path(visual),'visual_has_load_handles':visual.has_load_handles() if visual else None,
                               'mesh':path(player.get_editor_property('mesh').get_skeletal_mesh_asset())}
            for actor in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SovNPCCharacterBase):
                cls=path(actor.get_class())
                if not any(cls.endswith('BP_Aurelion'+r+'.BP_Aurelion'+r+'_C') for r in ('Linkbound','WallRunner','Weaver','Elite')): continue
                mesh=actor.get_editor_property('mesh')
                inst=mesh.get_anim_instance()
                pose={str(mesh.get_bone_name(i)):mesh.get_socket_transform(mesh.get_bone_name(i),unreal.RelativeTransformSpace.RTS_COMPONENT).export_text()
                      for i in (1, min(12,mesh.get_num_bones()-1)) if mesh.get_num_bones()>1}
                row['enemies'].append({'actor':path(actor),'class':cls,'pending_load':actor.is_character_pending_load(),
                    'mesh':path(mesh.get_skeletal_mesh_asset()),'anim_class':path(inst.get_class()) if inst else None,
                    'visible':mesh.is_visible(),'scale':mesh.get_relative_transform().export_text(),
                    'velocity':actor.get_velocity().export_text(),'pose':pose,
                    'montage':path(inst.get_current_active_montage()) if inst else None})
            REPORT['samples'].append(row)
        except Exception:
            import traceback
            REPORT['errors'].append(traceback.format_exc())
    if now-STATE['started']>=90.:
        unreal.unregister_slate_post_tick_callback(STATE['handle'])
        REPORT['status']='observation_complete' if not REPORT['errors'] else 'inspection_failed'
    (OUT/'visual-pie.json').write_text(json.dumps(REPORT,indent=2),encoding='utf8')

STATE['handle']=unreal.register_slate_post_tick_callback(tick)
unreal.log('ECLIPSE_VISUAL_PIE_OBSERVER_STARTED')
