"""Read-only enemy grants, montage data and installed blood system controls."""
import json
import os
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
report = {'roles': {}, 'blood': {}}
def export(asset, name):
    task = unreal.AssetExportTask()
    for key, value in {'object': asset, 'exporter': unreal.ObjectExporterT3D(),
        'filename': str(out / (name + '.t3d')), 'automated': True,
        'prompt': False, 'replace_identical': True}.items():
        task.set_editor_property(key, value)
    unreal.Exporter.run_asset_export_task(task)

for role in ('SecurityDrone','ContaminatedDrone','Enforcer','Linkbound','WallRunner','Weaver','Elite'):
    npc = unreal.load_asset('/Game/Aurelion/Enemies/NPC_Aurelion' + role)
    if not npc:
        continue
    config = npc.get_editor_property('ability_configuration')
    grants = list(config.get_editor_property('default_abilities'))
    report['roles'][role] = [g.get_path_name() for g in grants]
    export(npc, role + '_NPC')
    for grant in grants:
        export(unreal.get_default_object(grant), role + '_' + grant.get_name())
    data = unreal.load_asset('/Game/Aurelion/Enemies/Data/DA_Eclipse' + role + '_Melee') if role in ('Linkbound','WallRunner','Weaver','Elite') else None
    if data:
        export(data, role + '_MeleeData')

for name, path in {'Hit':'Hit/Niagara/NS_BulletHit_High',
    'Slash':'Slash/Niagara/NS_Slash_High', 'Burst':'Burst/Niagara/NS_BloodBurst_High',
    'Low':'Hit/Niagara/NS_BulletHit_Low'}.items():
    asset = unreal.load_asset('/Game/RealisticBlood/' + path)
    assert asset, path
    info = unreal.SovCombatFeedbackAuthoringLibrary.inspect_feedback_system(asset)
    report['blood'][name] = info.export_text()
    export(asset, 'Blood_' + name)
(out / 'enemy-combat-presentation.json').write_text(json.dumps(report, indent=2), encoding='utf8')
