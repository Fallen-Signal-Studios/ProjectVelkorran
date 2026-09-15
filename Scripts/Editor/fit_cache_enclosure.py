"""Reimport corrected owned assembly, preserve the map and review actual fit."""
from pathlib import Path
import json,os,runpy,unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);source=root/'Art/Source/Aurelion/Z08WallKit'
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());helper=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));before=helper['snapshot_actor_state'](actors)
spec=next(r for r in json.loads((source/'manifest.json').read_text())['modules'] if r['asset']=='SM_Aurelion_KIT_Z08WallAssembly')
dest='/Game/Aurelion/Environment/ArchitectureKit';materials={k:dest+'/Materials/M_AurelionKit_'+v for k,v in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal'}.items()}
helper['import_owned_mesh'](spec,source,dest+'/Meshes',materials)
assert helper['snapshot_actor_state'](actors)==before
result=runpy.run_path(str(root/'Scripts/Editor/check_cache_enclosure.py'))['check_cache_enclosure'](actors)
(out/'cache-enclosure-fit.json').write_text(json.dumps(dict(status='passed',settings=result,preserved_actor_states=len(actors)),indent=2))
code=(root/'Scripts/Editor/review_eclipse_wall_scars.py').read_text();prefix=code.split('views=[',1)[0];suffix=code.split('state=dict',1)[1]
views="views=[('enclosure-front',(-3050,22330,-1020),(-4,90),80),('enclosure-context',(-2670,22150,-975),(-8,125),90),('former-skins',(-2760,21890,-980),(-10,140),90)]\n"
exec(compile(prefix+views+'state=dict'+suffix,'cache_enclosure_review','exec'),globals())
