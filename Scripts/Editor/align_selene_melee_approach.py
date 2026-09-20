"""Match Selene's AI approach to the observed reach of her opening twin-blade swing."""
import json
import os
from pathlib import Path
import shutil
import unreal

assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY']) / 'selene-melee-approach'
out.mkdir(exist_ok=False)
path = '/Game/Aurelion/Characters/Melee/GA_Selene_MeleeLight'
asset = unreal.load_asset(path)
cdo = unreal.get_default_object(asset.generated_class())
assert isinstance(cdo, unreal.SovGameplayAbility_Melee)
definition = cdo.get_editor_property('attack_definition')
assert definition.validate() == ''
before = float(cdo.get_editor_property('default_bot_attack_range'))
assert before == 180., 'Review unexpected authored range before changing it'
source = Path(unreal.Paths.project_dir()) / 'Content/Aurelion/Characters/Melee/GA_Selene_MeleeLight.uasset'
shutil.copy2(source, out / source.name)
cdo.set_editor_property('default_bot_attack_range', 100.)
unreal.BlueprintEditorLibrary.compile_blueprint(asset)
assert float(unreal.get_default_object(asset.generated_class()).get_editor_property('default_bot_attack_range')) == 100.
assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
(out / 'result.json').write_text(json.dumps(dict(status='saved_requires_contact_verification', asset=path,
    before=before, after=100., reason='At 138 cm the opening twin-blade swing missed the stationary Weaver; approach closer using the existing AI range contract'), indent=2))
