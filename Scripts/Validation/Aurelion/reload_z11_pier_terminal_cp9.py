"""Public CP9 recovery from the independently inspected native M13 completion.

The normal-input validator stopped after a missed subsecond countdown sample,
but the request was accepted and the native final world has all 35 receipts.
This test starts from a live, native-completed M13 world. It changes no campaign
state except requesting one public checkpoint load per invocation.
"""
import hashlib
import json
import math
import os
import sys
import time
import traceback
from pathlib import Path
from uuid import uuid4

import unreal

root = Path(unreal.Paths.project_dir())
sys.path.insert(0,str(root/'Scripts/Validation/Aurelion'))
import probe_m13_native_checkpoint_reload as check

source = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])/'terminal-after-hold.json'
raw = source.read_bytes()
earned = json.loads(raw)
assert earned['status']=='terminal_snapshot_pass'
expected = earned['snapshot']
assert len(expected['journal'])==35 and expected['m13_complete']
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])/('CP9FromTerminal-'+uuid4().hex[:8])
out.mkdir()
maps = [root/'Content/Aurelion/Maps'/name for name in ('L_Aurelion_M12.umap','L_Aurelion_M13.umap')]
hashes = {str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in maps}
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world and 'L_Aurelion_M13' in world.get_name()
pc = unreal.GameplayStatics.get_player_controller(world,0)
pawn = unreal.GameplayStatics.get_player_pawn(world,0)
assert check.snapshot(world,pc,pawn)['journal']==expected['journal']
instance = unreal.GameplayStatics.get_game_instance(world)
saves = next(s for s in unreal.ObjectIterator(unreal.SovSaveSubsystem) if s.get_outer()==instance)
assert not saves.is_load_pending()
headers = [check.header_data(h) for h in saves.list_slots()
           if h.kind==unreal.SovSaveSlotKind.CHECKPOINT and h.slot_index==0]
assert len(headers)==1 and headers[0]['boundary']=='Aurelion.CP9'

report = dict(status='loading',source=str(source),source_sha256=hashlib.sha256(raw).hexdigest(),
              method='One public SovSaveSubsystem.load_slot(CHECKPOINT, 0) from the live earned final world',
              direct_journal_resource_or_transform_writes=False,preload_header=headers[0],callbacks=[],
              map_hashes_before=hashes)
state = dict(start=time.monotonic(),old_world=hash(world),stable_since=None,handle=None)
def write():
    (out/'cp9-from-terminal.json').write_text(json.dumps(report,indent=2,default=str),encoding='utf-8')
def loaded(result,header,message):
    report['callbacks'].append(dict(success=result==unreal.SovSaveResult.SUCCESS,
                                    header=check.header_data(header),message=str(message)))
    write()
def finish(error=None):
    report.update(status='failed' if error else 'passed_requires_visual_review',error=error,
                  elapsed_seconds=time.monotonic()-state['start'],
                  maps_unchanged=all(hashlib.sha256(p.read_bytes()).hexdigest()==hashes[str(p)] for p in maps))
    if not report['maps_unchanged']: report['status']='failed'
    saves.on_load_completed.remove_callable(loaded)
    if state['handle'] is not None:
        unreal.unregister_slate_post_tick_callback(state['handle'])
        state['handle']=None
    write()
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
def compare(actual):
    assert actual['journal']==expected['journal'] and actual['evidence']==expected['evidence']
    assert actual['facts']==expected['facts']
    assert actual['player_state']['stable_guid']==expected['player_state']['stable_guid']
    for role in ('player','companion'):
        assert actual[role]['stable_guid']==expected[role]['stable_guid'],role+' stable identity changed'
        assert actual[role]['items']==expected[role]['items'],role+' inventory changed'
        assert actual[role]['resources']==expected[role]['resources'],role+' resources changed'
        assert math.dist(actual[role]['transform']['location'],
                         expected[role]['transform']['location'])<6.,role+' departure moved'
    assert actual['lift']['state']==expected['lift']['state']
    assert not actual['paused']
def tick(_delta):
    try:
        now=time.monotonic()
        assert now-state['start']<90.,'CP9 reload did not settle in 90 seconds'
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if not world or hash(world)==state['old_world'] or saves.is_load_pending():return
        assert len(report['callbacks'])==1 and report['callbacks'][0]['success']
        assert report['callbacks'][0]['header']['boundary']=='Aurelion.CP9'
        pc=unreal.GameplayStatics.get_player_controller(world,0)
        pawn=unreal.GameplayStatics.get_player_pawn(world,0)
        if not pawn or not pawn.is_character_ready():return
        actual=check.snapshot(world,pc,pawn)
        compare(actual)
        assert not pc.is_move_input_ignored() and not pc.is_look_input_ignored()
        surfaces=[s for s in unreal.ObjectIterator(unreal.SovHolographicHUDSurface)
                  if s.get_world()==world and s.is_in_viewport()]
        assert len(surfaces)==1
        view=surfaces[0].get_holographic_hud_view()
        if not view.valid or check.tag(view.protagonist)!='Sov.Character.Player.Tarrik':
            state['stable_since']=None
            return
        if state['stable_since'] is None:state['stable_since']=now
        if now-state['stable_since']<4.:return
        report.update(snapshot=actual,stable_seconds=now-state['stable_since'],
                      one_hud=True,input_released=True)
        if 'shot_at' not in state:
            unreal.SystemLibrary.execute_console_command(world,
                'Shot showui -nosuffix filename='+str(out/'cp9-reload.png'))
            state['shot_at']=now
            write();return
        if now-state['shot_at']<2.:return
        assert (out/'cp9-reload.png').exists()
        finish()
    except Exception:
        finish(traceback.format_exc())

saves.on_load_completed.add_callable(loaded)
result,message=saves.load_slot(unreal.SovSaveSlotKind.CHECKPOINT,0)
assert result==unreal.SovSaveResult.LOAD_STARTED,str(message)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state['handle']=unreal.register_slate_post_tick_callback(tick)
write()
print('Z11_PIER_CP9_PUBLIC_LOAD_STARTED',out)
