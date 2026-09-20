"""Preview the reviewed Staccato BackB fit; scoped save only when SAVE_HOLSTER is true."""
import hashlib
import json
import os
import shutil
from pathlib import Path
import unreal

SLOT='Narrative.Equipment.Slot.Weapon.BackB'
BEFORE='(SocketName="Socket_BackB",Offset=(Rotation=(X=-0.000240,Y=-0.999968,Z=0.000543,W=0.007945),Translation=(X=-2.845216,Y=0.026159,Z=22.690775),Scale3D=(X=1.000000,Y=1.000000,Z=1.000000)))'
AFTER='(SocketName="Socket_BackB",Offset=(Rotation=(X=-0.001781,Y=-0.886395,Z=0.460270,W=0.049521),Translation=(X=-4.337210,Y=-16.430915,Z=29.897628),Scale3D=(X=1.000000,Y=1.000000,Z=1.000000)))'
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
project=Path(unreal.Paths.project_dir())
path=project/'Content/Items/Weapons/WI_Staccato.uasset'
protected=[project/'Content/Aurelion/Maps/L_Aurelion_M12.umap',project/'Content/WeaponMeshes/Weapon_Staccato.uasset']
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def configs(cdo,field):
    return {str(unreal.GameplayTagLibrary.get_tag_name(k)):v.export_text() for k,v in cdo.get_editor_property(field).items()}
def fingerprint(bp):return unreal.SovBlueprintAuthoringLibrary.fingerprint_blueprint(bp).partition('\n')[2]
def run():
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
    bp=unreal.load_asset('/Game/Items/Weapons/WI_Staccato')
    cdo=unreal.get_default_object(bp.generated_class())
    original=configs(cdo,'holster_attachment_configs');wield=configs(cdo,'wield_attachment_configs')
    assert original[SLOT] in (BEFORE,AFTER),'Unexpected holster customization; inspect before overwriting'
    protected_before={str(p):digest(p) for p in protected}
    original_fingerprint=fingerprint(bp)
    updated={};restore={}
    for key,config in cdo.get_editor_property('holster_attachment_configs').items():
        copy=type(config)();assert copy.import_text(config.export_text());restore[key]=copy
        revised=type(config)();assert revised.import_text(AFTER if str(unreal.GameplayTagLibrary.get_tag_name(key))==SLOT else config.export_text())
        updated[key]=revised
    cdo.set_editor_property('holster_attachment_configs',updated)
    actual=configs(cdo,'holster_attachment_configs')
    assert actual==dict(original,**{SLOT:AFTER})
    assert configs(cdo,'wield_attachment_configs')==wield
    # Removing our one field edit must reproduce the entire original reflected state.
    cdo.set_editor_property('holster_attachment_configs',restore)
    assert fingerprint(bp)==original_fingerprint,'Unrelated Blueprint defaults changed'
    cdo.set_editor_property('holster_attachment_configs',updated)
    report=dict(before=original,after=actual,wield_preserved=wield,saved=False,file_before=digest(path))
    if globals().get('SAVE_HOLSTER',False) and original!=actual:
        shutil.copy2(path,out/'WI_Staccato.before.uasset')
        bp.modify();cdo.modify()
        assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
        report['saved']=True
    assert {str(p):digest(p) for p in protected}==protected_before
    report.update(file_after=digest(path),protected=protected_before,status='passed')
    (out/'staccato-holster-fit.json').write_text(json.dumps(report,indent=2))
if __name__=='__main__':run()
