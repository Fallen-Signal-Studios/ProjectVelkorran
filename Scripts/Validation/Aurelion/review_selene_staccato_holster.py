"""Fresh CP2 holster fit, physical socket, movement and draw/stow review."""
import json
import os
import runpy
from pathlib import Path
import unreal
project=Path(unreal.Paths.project_dir())
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
author=runpy.run_path(str(project/'Scripts/Editor/refine_selene_staccato_holster.py'))
bp=unreal.load_asset('/Game/Items/Weapons/WI_Staccato')
cdo=unreal.get_default_object(bp.generated_class())
assert author['configs'](cdo,'holster_attachment_configs')[author['SLOT']]==author['AFTER']
rows=[]
def sample(pawn,case,world):
    matches=[]
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.WeaponVisual):
        attach=actor.get_editor_property('attach_state')
        if attach.get_editor_property('char_owner')!=pawn:continue
        item=attach.get_editor_property('weapon_owner')
        if not item or item.get_class()!=bp.generated_class():continue
        matches.append(actor)
        wielded=item in pawn.get_wielded_weapons()
        assert wielded==(case=='WI_Staccato')
        field='wield_attachment_configs' if wielded else 'holster_attachment_configs'
        slot='Narrative.Equipment.WieldSlot.Mainhand' if wielded else author['SLOT']
        config=next(v for k,v in item.get_editor_property(field).items()
                    if str(unreal.GameplayTagLibrary.get_tag_name(k))==slot)
        row=dict(case=case,wielded=wielded,actor=actor.get_path_name(),meshes=[])
        for mesh in actor.get_components_by_class(unreal.SkeletalMeshComponent):
            assert str(mesh.get_attach_socket_name())==str(config.get_editor_property('socket_name'))
            actual=mesh.get_relative_transform();expected=config.get_editor_property('offset')
            assert (actual.translation-expected.translation).length()<.01
            assert (actual.scale3d-expected.scale3d).length()<.001
            dot=abs(sum(getattr(actual.rotation,k)*getattr(expected.rotation,k) for k in ('x','y','z','w')))
            assert dot>.99999
            row['meshes'].append(dict(name=mesh.get_name(),socket=str(mesh.get_attach_socket_name()),
                                      relative=actual.export_text()))
        assert len(row['meshes'])==2
        rows.append(row)
    assert len(matches)==1,'Expected one Staccato visual owned by the active Selene'
    (out/'staccato-live-fit.json').write_text(json.dumps(rows,indent=2))
runpy.run_path(str(Path(__file__).with_name('review_selene_posture_play.py')),init_globals={
    'POSTURE_REVIEW_CASES':('restored','crouch','walk','jump','landed','WI_Verity','WI_Staccato','restowed'),
    'POSTURE_SAMPLE_HOOK':sample})
