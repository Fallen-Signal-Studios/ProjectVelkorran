"""Bind owned enemy archetypes to existing authored native sustain pickups.

Stopped-editor content only. Fatal damage and native overlap own all runtime
spawning/collection. No actors, damage, resources or campaign receipts are created.
"""
import json
from pathlib import Path
import unreal

assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Aurelion_M12'
root=Path(__file__).resolve().parents[2]
out=root/'Saved/Validation/Aurelion/SustainFallback-20260913'; out.mkdir(parents=True,exist_ok=True)
assets=unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
ammo=unreal.load_asset('/Game/Ammo/BP_AmmoPickup').generated_class()
echo=unreal.load_asset('/Game/Ammo/BP_EchoPickup').generated_class()
ammo_cdo=unreal.get_default_object(ammo); echo_cdo=unreal.get_default_object(echo)
assert isinstance(ammo_cdo,unreal.SovAmmoCombatSustainPickup)
assert isinstance(echo_cdo,unreal.SovEchoCombatSustainPickup)
rifle=unreal.get_default_object(unreal.load_asset('/Game/Items/Weapons/WI_Cinderline').generated_class()).get_editor_property('required_ammo')
assert rifle and ammo_cdo.get_editor_property('ammo_item_class')==rifle
amount=ammo_cdo.get_editor_property('ammo_quantity')
echo_amount=echo_cdo.get_editor_property('echo_amount')
assert amount==63 and echo_amount==4, 'Existing pickup payload changed; review tuning before applying'
desired=dict(ammo_pickup_class=ammo,ammo_item_class=rifle,ammo_amount=amount,
             echo_pickup_class=echo,echo_amount=echo_amount)
roles=('SecurityDrone','ContaminatedDrone','Enforcer','Linkbound','WallRunner','Weaver','Elite')
report=dict(status='authoring',native_runtime_qualified=False,archetypes=[],placed_instances=[],
    payload_source='Existing /Game/Ammo pickup Blueprint defaults: 63 rifle rounds, 4 Echo; 20s native lifetime unchanged')
def encode(v):
    return v.get_path_name() if isinstance(v,unreal.Object) else v
def read(c): return {k:encode(c.get_editor_property(k)) for k in desired}
def apply(c):
    assert c and c.get_editor_property('combat_sustain_drops_enabled') and c.get_editor_property('drop_ammo') and c.get_editor_property('drop_echo')
    for key in ('ammo_pickup_class','ammo_item_class','echo_pickup_class'):
        assert c.get_editor_property(key) in (None,desired[key]), 'Unexpected existing binding: '+key
    for key,value in desired.items(): c.set_editor_property(key,value)
    assert read(c)=={k:encode(v) for k,v in desired.items()}
try:
    classes=[]
    for role in roles:
        bp=unreal.load_asset('/Game/Aurelion/Enemies/BP_Aurelion'+role); assert bp
        cdo=unreal.get_default_object(bp.generated_class())
        c=cdo.get_component_by_class(unreal.SovCombatSustainDropComponent)
        row=dict(role=role,before=read(c)); report['archetypes'].append(row)
        apply(c)
        assert unreal.SovAurelionEnemyAuthoringLibrary.compile_owned_blueprint(bp)
        c=unreal.get_default_object(bp.generated_class()).get_component_by_class(unreal.SovCombatSustainDropComponent)
        assert read(c)=={k:encode(v) for k,v in desired.items()}, role
        row['after']=read(c)
        assert assets.save_loaded_asset(bp,only_if_is_dirty=False)
        classes.append(bp.generated_class())
    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        if actor.get_class() not in classes: continue
        c=actor.get_component_by_class(unreal.SovCombatSustainDropComponent)
        before=read(c); apply(c)
        report['placed_instances'].append(dict(label=actor.get_actor_label(),before=before,after=read(c)))
    assert len(report['placed_instances'])>=20, 'Unexpectedly incomplete placed enemy census'
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    report['status']='SAVED_CONTENT_RUNTIME_PENDING'
except Exception as error:
    report.update(status='FAILED',error=str(error)); raise
finally:
    (out/'configured.json').write_text(json.dumps(report,indent=2),encoding='utf8')
unreal.log('AURELION_SUSTAIN_CONTENT_SAVED '+str(len(report['placed_instances'])))
