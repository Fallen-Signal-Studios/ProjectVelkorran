"""Read-only Cinderline weapon/ability defaults and Blueprint graph export."""
import json
import os
from pathlib import Path
import unreal
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])/'cinderline-audit'
out.mkdir(parents=True, exist_ok=True)
paths = ['/Game/Items/Weapons/WI_Cinderline',
         '/NarrativePro/Pro/Core/Abilities/GameplayAbilities/Attacks/Firearms/GA_Firearm_Cinderline',
         '/NarrativePro/Pro/Core/Abilities/GameplayEffects/GE_WeaponDamage']
report = {'exports': [], 'errors': []}
try:
    for path in paths:
        asset = unreal.load_asset(path)
        assert asset, path
        cdo = unreal.get_default_object(asset.generated_class())
        if asset.get_name() == 'GE_WeaponDamage':
            report['effect_components'] = [c.get_path_name() for c in cdo.get_editor_property('ge_components')]
        if asset.get_name() == 'GA_Firearm_Cinderline':
            report['damage_effect'] = cdo.get_editor_property('Damage Effect Class').get_path_name()
        if asset.get_name() == 'WI_Cinderline':
            report['grants'] = {key:[c.get_path_name() for c in cdo.get_editor_property(key)]
                for key in ('weapon_abilities', 'mainhand_weapon_abilities', 'offhand_weapon_abilities')}
        for obj, suffix in ((asset, ''), (unreal.get_default_object(asset.generated_class()), '-defaults')):
            task = unreal.AssetExportTask()
            filename = out/(asset.get_name()+suffix+'.t3d')
            for key, value in {'object':obj, 'exporter':unreal.ObjectExporterT3D(), 'filename':str(filename),
                              'automated':True, 'prompt':False, 'selected':False, 'replace_identical':True}.items():
                task.set_editor_property(key, value)
            assert unreal.Exporter.run_asset_export_task(task)
            report['exports'].append(str(filename))
except Exception:
    import traceback
    report['errors'].append(traceback.format_exc())
(out/'report.json').write_text(json.dumps(report, indent=2), encoding='utf8')
