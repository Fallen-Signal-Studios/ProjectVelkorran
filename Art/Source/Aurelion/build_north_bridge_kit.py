"""Build fitted north spans with shared deck end planes and no actor scaling."""
from pathlib import Path
import json,runpy
root=Path(__file__).resolve().parent
fit=json.loads((root/'NorthBridgeKit/bridge-fit.json').read_text())
modules=[]
for b in fit['baseline']:
    folder=root/'NorthBridgeKit'/b['asset_prefix']
    runpy.run_path(str(root/'build_approach_bridge_kit.py'),init_globals=dict(KIT_ROOT=str(folder),SPAN_LENGTH=b['length_m'],MESH_PREFIX=b['asset_prefix']))
    for spec in json.loads((folder/'manifest.json').read_text())['modules']:
        spec['source_subdir']=b['asset_prefix'];modules.append(spec)
(root/'NorthBridgeKit/manifest.json').write_text(json.dumps(dict(status='Fitted north bridge candidate; Unreal and live acceptance pending',modules=modules),indent=2))
