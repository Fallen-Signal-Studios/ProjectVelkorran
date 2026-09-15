"""Reimport owned wall geometry without changing any saved placement or collision."""
from pathlib import Path
import hashlib,json,os,runpy,unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
source=root/'Art/Source/Aurelion/Z09WallKit'
map_file=root/'Content/Aurelion/Maps/L_Aurelion_M12.umap';map_hash=hashlib.sha256(map_file.read_bytes()).hexdigest()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());assert len(actors)==3140
helper=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
before=helper['snapshot_actor_state'](actors)
dest='/Game/Aurelion/Environment/ArchitectureKit'
materials={k:dest+'/Materials/M_AurelionKit_'+v for k,v in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal'}.items()}
manifest=json.loads((source/'manifest.json').read_text())
for spec in manifest['modules']:
    assert not json.loads((source/(spec['asset'].replace('SM_Aurelion_KIT_','')+'-coplanar.json')).read_text())['overlaps']
    if 'Lintel' not in spec['asset'] and not globals().get('SKIP_WALL_DETAIL_IMPORT',False):helper['import_owned_mesh'](spec,source,dest+'/Meshes',materials)
assert helper['snapshot_actor_state'](actors)==before
result=runpy.run_path(str(root/'Scripts/Editor/check_z09_walls.py'))['check_z09_walls'](actors)
assert hashlib.sha256(map_file.read_bytes()).hexdigest()==map_hash
(out/'map-file-preservation.json').write_text(json.dumps(dict(status='unchanged',sha256=map_hash,editor_dirty_after_import=bool(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())),indent=2))
(out/'z09-wall-detail-fit.json').write_text(json.dumps(result,indent=2))
capture=(root/'Scripts/Editor/review_eclipse_wall_scars.py').read_text();prefix,suffix=capture.split('views=[',1)[0],capture.split('state=dict',1)[1]
views="views=[('entry',(-300,25750,-1330),(12,90),80),('close',(-300,26350,-1250),(0,90),80),('reverse',(350,30200,-1330),(12,-90),80)]\n"
exec(compile(prefix+views+'state=dict'+suffix,'z09_wall_detail_capture','exec'),globals())
