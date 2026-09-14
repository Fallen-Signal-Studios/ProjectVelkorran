"""Create owned Parasites locomotion copies. Does not change character assignments."""
import json
from pathlib import Path
import unreal

OUT = Path(unreal.Paths.project_dir()).resolve()/'Saved/Validation/Aurelion/EclipseAnimation'
OUT.mkdir(parents=True, exist_ok=True)
ROOT = '/Game/Aurelion/Enemies/Animation/'
PACK = '/Game/Parasites_Pack/'
SOURCE_BP = ROOT+'ABP_EclipseLinkbound'
SOURCE_BS = PACK+'Animations/Parasites/BS_Linkbound'
ROLES = {
    'WallRunner': ('SK_Parasite_Spider', 'Parasite_Spider/', 'Anim_Spider_', 'Idle', 'Idle_other'),
    'Weaver': ('SK_Alfa', 'Parasites_Alfa/Animations/', 'Anim_Alfa_', 'Idle', 'Idle'),
    'Elite': ('SK_Fat', 'Parasites_Fat/', 'Anim_Fat_', 'idle', 'idle_other'),
}
report = {'status':'running', 'roles':{}, 'saved':[], 'gameplay_qualified':False}
try:
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
    source_sequences = [unreal.load_asset(PACK+'Animations/Parasites/Anim_Parazite_'+n)
                        for n in ('Walk','Run','Run1','Idle','Idle_Other')]
    for role,(mesh_name,folder,prefix,idle,other) in ROLES.items():
        mesh=unreal.load_asset(PACK+'Mesh/'+mesh_name)
        skeleton=mesh.get_editor_property('skeleton')
        bp_path=ROOT+'ABP_Eclipse'+role
        bs_path=ROOT+'BS_Eclipse'+role
        assert not unreal.EditorAssetLibrary.does_asset_exist(bp_path), 'Refusing to overwrite '+bp_path
        assert not unreal.EditorAssetLibrary.does_asset_exist(bs_path), 'Refusing to overwrite '+bs_path
        replacements=[unreal.load_asset(PACK+'Animations/'+folder+prefix+n)
                      for n in ('Walk','Run','Run',idle,other)]
        assert all(replacements) and all(source_sequences)
        bp=unreal.EditorAssetLibrary.duplicate_asset(SOURCE_BP,bp_path)
        bs=unreal.EditorAssetLibrary.duplicate_asset(SOURCE_BS,bs_path)
        result=unreal.SovEclipseAnimationAuthoringLibrary.adapt_copied_animation(
            bp,bs,skeleton,source_sequences+[unreal.load_asset(SOURCE_BS)],replacements+[bs])
        row={'mesh':mesh.get_path_name(),'skeleton':skeleton.get_path_name(),'blueprint':bp_path,
             'blendspace':bs_path,'compile':result,'bones':[str(n) for n in unreal.SovEclipseAnimationAuthoringLibrary.mesh_bones(mesh)]}
        report['roles'][role]=row
        assert result.startswith('OK:'), result
        for asset in (bs,bp):
            assert unreal.EditorAssetLibrary.save_loaded_asset(asset,only_if_is_dirty=False)
            report['saved'].append(asset.get_path_name())
    attacks=unreal.load_asset(PACK+'Mesh/DA_EclipseLinkboundAttacks')
    if attacks:
        report['existing_attack_asset_class']=attacks.get_class().get_path_name()
        task=unreal.AssetExportTask()
        for k,v in {'object':attacks,'exporter':unreal.ObjectExporterT3D(),'filename':str(OUT/'existing-attacks.t3d'),
                    'automated':True,'prompt':False,'replace_identical':True}.items(): task.set_editor_property(k,v)
        unreal.Exporter.run_asset_export_task(task)
    report['status']='prepared_not_gameplay_qualified'
except Exception:
    import traceback
    report['status']='failed'; report['error']=traceback.format_exc()
finally:
    (OUT/'prepare-presentation.json').write_text(json.dumps(report,indent=2),encoding='utf8')
    unreal.log('ECLIPSE_PRESENTATION_PREPARE '+report['status'])
