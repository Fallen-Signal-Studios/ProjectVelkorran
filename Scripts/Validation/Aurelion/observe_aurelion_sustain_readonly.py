"""Observe native pickup appearance/claims and actual player reserves; no writes to gameplay."""
import json
import os
from pathlib import Path
import time
import traceback
import unreal

_RUN=None

class Observer:
    def __init__(self):
        self.out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])/'sustain-observation.json'
        assert not self.out.exists(), 'Preserve the existing observation'
        self.started=time.monotonic(); self.last_tick=0.; self.last_write=0.
        self.handle=None; self.seen_world=False; self.last_signature=None
        self.report=dict(status='running',read_only=True,samples=[],errors=[],
            scope='Observed native pickup states and current player ammunition/Echo only; no fabricated damage, resources, actor motion, collection or mission progress')
    def write(self):
        self.report['elapsed_seconds']=round(time.monotonic()-self.started,3)
        self.out.write_text(json.dumps(self.report,indent=2),encoding='utf8')
    def stop(self,reason):
        if self.handle is not None:
            unreal.unregister_slate_post_tick_callback(self.handle); self.handle=None
        self.report.update(status='stopped',reason=reason); self.write()
    def tick(self,delta):
        now=time.monotonic()
        if now-self.last_tick<.1: return
        self.last_tick=now
        try:
            world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
            if not world:
                if self.seen_world: self.stop('PIE ended')
                elif now-self.started>180: self.stop('No PIE world appeared')
                return
            assert 'L_Aurelion_M12' in world.get_name(), 'Observer scope ends at M12'
            self.seen_world=True
            pawn=unreal.GameplayStatics.get_player_pawn(world,0)
            player=None
            if pawn and isinstance(pawn,unreal.SovPlayerCharacterBase) and pawn.is_character_ready():
                weapons=[w for w in pawn.get_wielded_weapons() if w and 'WI_Cinderline' in w.get_class().get_path_name()]
                player=dict(actor=pawn.get_path_name(),health=pawn.get_health(),
                    echo=pawn.get_echo_component().get_echo(),
                    ammunition=[dict(clip=w.get_ammo_in_clip(),reserve=w.get_spare_ammo()) for w in weapons])
            pickups=[]
            for actor in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SovCombatSustainPickup):
                owner=actor.get_owner(); p=actor.get_actor_location()
                row=dict(actor=actor.get_path_name(),owner=owner.get_path_name() if owner else None,
                    claimed=actor.is_claimed(),position=[round(p.x,1),round(p.y,1),round(p.z,1)],
                    distance_to_player=(p-pawn.get_actor_location()).length() if pawn else None)
                if isinstance(actor,unreal.SovAmmoCombatSustainPickup):
                    row.update(kind='ammo',quantity=actor.get_ammo_quantity(),ammo=actor.get_ammo_item_class().get_path_name())
                else: row.update(kind='echo',quantity=float(actor.get_editor_property('echo_amount')))
                pickups.append(row)
            signature=json.dumps(dict(player=player,pickups=[{k:v for k,v in r.items() if k not in ('position','distance_to_player')} for r in pickups]),sort_keys=True)
            if signature!=self.last_signature:
                self.last_signature=signature
                self.report['samples'].append(dict(elapsed=round(now-self.started,3),player=player,pickups=pickups))
            if now-self.last_write>1.: self.last_write=now; self.write()
            if now-self.started>1800: self.stop('Thirty-minute observation bound')
        except Exception:
            self.report['errors'].append(traceback.format_exc()); self.stop('Read-only observation error')

def start():
    global _RUN
    assert _RUN is None, 'Observer already exists'
    _RUN=Observer(); _RUN.write()
    _RUN.handle=unreal.register_slate_post_tick_callback(_RUN.tick)
    return _RUN
