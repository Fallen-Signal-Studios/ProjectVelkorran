"""Grant existing ammo before firearms so WeaponItem.AddedToInventory can fill clips.
Changes ordering only; no added ammunition, weapon defaults, saved inventory or C++.
"""
import unreal,os,json,shutil
from pathlib import Path
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
report=[]
for hero,ammo_names,count in [('Tarrik',['Ammo_Rifle'],3),('Selene',['Ammo_Pistol','Ammo_Rifle'],5)]:
    path='/Game/Items/Loadouts/IC_'+hero
    asset=unreal.load_asset(path)
    rows=list(asset.get_editor_property('Items'))
    before=[r.export_text() for r in rows]
    assert len(rows)==count,'Unexpected loadout: '+hero
    def is_ammo(row):
        text=row.export_text()
        return any('/'+name+'.'+name+'_C' in text for name in ammo_names)
    ammo=[r for r in rows if is_ammo(r)]
    assert len(ammo)==len(ammo_names),hero
    reordered=ammo+[r for r in rows if not is_ammo(r)]
    after=[r.export_text() for r in reordered]
    assert sorted(before)==sorted(after)
    source=Path(unreal.Paths.project_dir())/'Content/Items/Loadouts'/('IC_'+hero+'.uasset')
    backup=out/('IC_'+hero+'.before.uasset')
    if not backup.exists():shutil.copy2(source,backup)
    asset.set_editor_property('Items',reordered)
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset,only_if_is_dirty=False)
    assert [r.export_text() for r in asset.get_editor_property('Items')]==after
    report.append(dict(hero=hero,before=before,after=after))
(out/'loadout-ammo-order.json').write_text(json.dumps(dict(status='saved',loadouts=report),indent=2))
