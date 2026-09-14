"""Author compatible parasite montages/data around the existing native melee payload."""
import json
import shutil
from pathlib import Path
import unreal

PROJECT=Path(unreal.Paths.project_dir()).resolve()
OUT=PROJECT/'Saved/Validation/Aurelion/EclipseAnimation'
ROOT='/Game/Aurelion/Enemies/'
PACK='/Game/Parasites_Pack/'
ROLES={
    'Linkbound':('SK_Parasit','Parasites/Anim_Parazite_Attack_1',18.),
    'WallRunner':('SK_Parasite_Spider','Parasite_Spider/Anim_Spider_Attack_1',18.),
    'Weaver':('SK_Alfa','Parasites_Alfa/Animations/Anim_Alfa_Attack_1',18.),
    'Elite':('SK_Fat','Parasites_Fat/Anim_Fat_Attack_1',30.),
}
report={'status':'running','roles':{},'saved':[],'combat_qualified':False}
tools=unreal.AssetToolsHelpers.get_asset_tools()

def create(path,cls,factory):
    assert not unreal.EditorAssetLibrary.does_asset_exist(path),'Refusing overwrite '+path
    folder,name=path.rsplit('/',1)
    asset=tools.create_asset(name,folder,cls,factory)
    assert asset,path
    return asset

try:
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
    for role,(mesh_name,sequence_path,damage) in ROLES.items():
        definition=unreal.load_asset(ROOT+'NPC_Aurelion'+role)
        original=definition.get_editor_property('ability_configuration')
        mesh=unreal.load_asset(PACK+'Mesh/'+mesh_name)
        sequence=unreal.load_asset(PACK+'Animations/'+sequence_path)
        bones={str(n) for n in unreal.SovEclipseAnimationAuthoringLibrary.mesh_bones(mesh)}
        start,end='CATRigLArmPalm','CATRigRArmPalm'
        assert start in bones and end in bones,role+' missing sweep bones'
        assert sequence.get_editor_property('skeleton')==mesh.get_editor_property('skeleton')
        factory=unreal.AnimMontageFactory()
        factory.set_editor_property('target_skeleton',mesh.get_editor_property('skeleton'))
        factory.set_editor_property('source_animation',sequence)
        montage=create(ROOT+'Animation/AM_Eclipse'+role+'_Attack',unreal.AnimMontage,factory)
        data_factory=unreal.DataAssetFactory()
        data_factory.set_editor_property('data_asset_class',unreal.SovMeleeAttackDefinition)
        data=create(ROOT+'Data/DA_Eclipse'+role+'_Melee',unreal.SovMeleeAttackDefinition,data_factory)
        duration=float(sequence.get_play_length())
        assert .3<duration<=5.,'Attack duration needs manual authoring'
        node=unreal.SovMeleeAttackNode()
        for name,value in {'montage':montage,'start_socket':start,'end_socket':end,'trace_radius':14.,
            'startup':duration*.3,'active':duration*.3,'recovery':duration*.4,
            'branch_open':duration*.65,'branch_close':duration*.95,'damage':damage,
            'poise_damage':20. if role=='Elite' else 10.}.items(): node.set_editor_property(name,value)
        data.set_editor_property('nodes',[node])
        valid=data.validate()
        # UE Python exposes a bool + one out parameter as the out value on
        # success, or None on failure. Validate's successful Error is empty.
        assert valid == '',repr(valid)
        factory=unreal.BlueprintFactory()
        factory.set_editor_property('parent_class',unreal.SovGameplayAbility_Melee)
        ability=create(ROOT+'Abilities/GA_Eclipse'+role+'_Melee',unreal.Blueprint,factory)
        cdo=unreal.get_default_object(ability.generated_class())
        cdo.set_editor_property('attack_definition',data)
        cdo.set_editor_property('allow_unarmed',True)
        # Existing native range/frequency and ability-tag arbitration remain authoritative.
        assert unreal.SovAurelionEnemyAuthoringLibrary.compile_owned_blueprint(ability)
        config_path=ROOT+'AC_Abilities_Eclipse'+role
        assert not unreal.EditorAssetLibrary.does_asset_exist(config_path),config_path
        config=unreal.EditorAssetLibrary.duplicate_asset(original.get_path_name(),config_path)
        grants=list(config.get_editor_property('default_abilities'))
        old_name='GA_Melee_Punch_Linkbound_C' if role=='Linkbound' else 'GA_Melee_Punch_Unarmed_C'
        removed=[g for g in grants if g.get_name()==old_name]
        assert len(removed)==1,'Expected exactly the old unarmed attack grant'
        grants=[g for g in grants if g not in removed]+[ability.generated_class()]
        config.set_editor_property('default_abilities',grants)
        disk=PROJECT/'Content/Aurelion/Enemies'/('NPC_Aurelion'+role+'.uasset')
        backup=OUT/'before-melee'/disk.name
        backup.parent.mkdir(parents=True,exist_ok=True)
        if not backup.exists(): shutil.copy2(disk,backup)
        for asset in (montage,data,ability,config):
            assert unreal.EditorAssetLibrary.save_loaded_asset(asset,only_if_is_dirty=False)
            report['saved'].append(asset.get_path_name())
        definition.set_editor_property('ability_configuration',config)
        assert unreal.EditorAssetLibrary.save_loaded_asset(definition,only_if_is_dirty=False)
        report['saved'].append(definition.get_path_name())
        report['roles'][role]={'original_configuration':original.get_path_name(),'configuration':config_path,
            'replaced_grant':removed[0].get_path_name(),'ability':ability.get_path_name(),
            'node':node.export_text(),'duration':duration,'validation':str(valid),
            'timing_status':'provisional; must be checked against the visible swing in PIE'}
    report['status']='authored_requires_combat_validation'
except Exception:
    import traceback
    report['status']='failed';report['error']=traceback.format_exc()
finally:
    (OUT/'native-melee-authoring.json').write_text(json.dumps(report,indent=2),encoding='utf8')
    unreal.log('ECLIPSE_NATIVE_MELEE '+report['status'])
