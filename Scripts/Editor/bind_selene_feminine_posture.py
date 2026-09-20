"""Install the native Selene posture in the original base class; save only that asset."""
import hashlib
import json
import os
import shutil
import traceback
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
project = Path(unreal.Paths.project_dir())
package = project / 'Plugins/Narrativeed3f9374a6eV6/Content/Pro/Core/Character/Biped/Animation/ABP/Base/ABP_Biped.uasset'
protected_map = project / 'Content/Aurelion/Maps/L_Aurelion_M12.umap'
report = dict(status='preflight', gameplay_verified=False)
digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
try:
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    report['m12_before'] = digest(protected_map)
    report['base_before'] = digest(package)
    shutil.copy2(package, out / 'ABP_Biped.before.uasset')
    bp = unreal.load_asset('/NarrativePro/Pro/Core/Character/Biped/Animation/ABP/Base/ABP_Biped')
    blend = unreal.load_asset('/Game/Characters/Animation/SeleneGASPALS/BS_SeleneFemininePostureDelta')
    assert bp and blend
    result = unreal.SovBlueprintAuthoringLibrary.configure_selene_feminine_posture(bp, blend)
    report['compile'] = result.report
    assert result.succeeded, result.report
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=True)
    report['base_after'] = digest(package)
    assert report['base_after'] != report['base_before']
    assert digest(protected_map) == report['m12_before']
    report['status'] = 'bound_requires_gameplay_review'
except Exception:
    report['status'] = 'failed'
    report['error'] = traceback.format_exc()
finally:
    (out / 'selene-posture-binding.json').write_text(json.dumps(report, indent=2))
if report['status'] == 'failed':
    raise RuntimeError(report['error'])
