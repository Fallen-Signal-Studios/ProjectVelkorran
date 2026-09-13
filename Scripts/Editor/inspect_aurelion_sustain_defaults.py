"""Read-only enemy sustain and fallback weapon authoring census."""
import json
from pathlib import Path
import unreal

assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
root=Path(__file__).resolve().parents[2]
out=root/'Saved/Validation/Aurelion/SustainFallback-20260913'
out.mkdir(parents=True,exist_ok=True)
def encode(v):
    if isinstance(v,unreal.Object): return v.get_path_name()
    if v is None or isinstance(v,(str,bool,int,float)): return v
    return v.export_text() if hasattr(v,'export_text') else str(v)
def properties(obj,names):
    row={}
    for name in names:
        try: row[name]=encode(obj.get_editor_property(name))
        except Exception as e: row[name+'_error']=str(e)
    return row
rows=[]
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    component=actor.get_component_by_class(unreal.SovCombatSustainDropComponent)
    if not component: continue
    rows.append(dict(label=actor.get_actor_label(),actor_class=actor.get_class().get_path_name(),
        drops=properties(component,['combat_sustain_drops_enabled','drop_ammo','ammo_pickup_class',
        'ammo_item_class','ammo_amount','drop_echo','echo_pickup_class','echo_amount'])))
weapons={}
for name in ('Cinderline','Velkorran'):
    bp=unreal.load_asset('/Game/Items/Weapons/WI_'+name); assert bp
    cdo=unreal.get_default_object(bp.generated_class())
    weapons[name]=properties(cdo,['required_ammo','clip_size','allow_manual_reload','attack_range',
        'weapon_visual_class','weapon_type','weapon_abilities'])
    weapons[name]['class']=cdo.get_class().get_path_name()
pickups={}
for name in ('Ammo','Echo'):
    bp=unreal.load_asset('/Game/Ammo/BP_'+name+'Pickup'); assert bp
    cdo=unreal.get_default_object(bp.generated_class())
    row=properties(cdo,['pickup_lifetime_seconds','pickup_radius','ammo_item_class','ammo_quantity','echo_amount'])
    row['class']=cdo.get_class().get_path_name()
    row['native_type']=isinstance(cdo,unreal.SovCombatSustainPickup)
    mesh=cdo.get_component_by_class(unreal.StaticMeshComponent)
    row['mesh']=encode(mesh.static_mesh) if mesh else None
    pickups[name]=row
(out/'defaults.json').write_text(json.dumps(dict(enemies=rows,weapons=weapons,pickups=pickups),indent=2),encoding='utf8')
unreal.log('SUSTAIN_DEFAULTS_READ_ONLY '+str(len(rows)))
