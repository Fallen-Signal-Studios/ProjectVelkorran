import os
from pathlib import Path
import unreal
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
root = '/NarrativePro/Pro/Core/'
for path in [
    root + 'Weapons/Rifle/Animations/3P/AM_MM_Rifle_Fire',
    root + 'Weapons/Pistol/Animations/3P/AM_MM_Pistol_Fire',
    root + 'Character/Biped/Animation/Sequences/Weapon/Sword/1H/GrenadeThrow',
    root + 'Character/Biped/Animation/Sequences/Weapon/Sword/2H/3P/AM_Sword_2h_Swipe_01',
    root + 'Character/Biped/Animation/Sequences/Weapon/Sword/2H/3P/AM_Sword_2h_Swipe_02',
    '/NarrativePro/Pro/Demo/Cinematics/TestAnims/Throw_ue5',
]:
    try:
        asset = unreal.load_asset(path)
        task = unreal.AssetExportTask()
        for key, value in dict(object=asset, exporter=unreal.ObjectExporterT3D(),
                filename=str(out / (asset.get_name() + '.t3d')), automated=True,
                prompt=False, selected=False, replace_identical=False).items():
            task.set_editor_property(key, value)
        assert unreal.Exporter.run_asset_export_task(task)
    except Exception as exc:
        unreal.log_warning(str(exc))
