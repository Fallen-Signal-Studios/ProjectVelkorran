"""Keep ordinary mission controls ahead of competing Aurelion corpse loot prompts."""
import json
import os
import shutil
import hashlib
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
roles = ('Enforcer', 'Linkbound', 'SecurityDrone', 'ContaminatedDrone',
         'WallRunner', 'Weaver', 'Elite')
map_file = Path(unreal.Paths.project_dir()) / 'Content/Aurelion/Maps/L_Aurelion_M12.umap'
map_hash = hashlib.sha256(map_file.read_bytes()).hexdigest()
report = dict(status='running', changes=[], qualification='Saved defaults only; fresh runtime verification required.')

def interactable(bp):
    cdo = unreal.get_default_object(bp.generated_class())
    components = [c for c in cdo.get_components_by_class(unreal.NarrativeInteractableComponent)
                  if c.get_name() == 'NPCInteractable']
    assert len(components) == 1, bp.get_path_name()
    return components[0]

for role in roles:
    name = 'BP_Aurelion' + role
    bp = unreal.load_asset('/Game/Aurelion/Enemies/' + name)
    component = interactable(bp)
    before = component.get_editor_property('interaction_priority')
    assert before in (0, -2), (name, before)
    source = Path(unreal.Paths.project_dir()) / 'Content/Aurelion/Enemies' / (name + '.uasset')
    target = out / (name + '.before.uasset')
    assert not target.exists()
    shutil.copy2(source, target)
    # Native selector weights each priority step by four. Two steps outweigh
    # facing/distance among admitted in-reach candidates. Admission, range,
    # loot contents and interaction callbacks remain owned by Narrative.
    component.set_editor_property('interaction_priority', -2)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert interactable(bp).get_editor_property('interaction_priority') == -2
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
    report['changes'].append(dict(asset=bp.get_path_name(), before=before, after=-2))
    (out / 'loot-priority-authoring.json').write_text(json.dumps(report, indent=2))
# Blueprint reinstancing may dirty loaded actor instances. Never save the map.
report['unsaved_map_packages'] = [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
assert hashlib.sha256(map_file.read_bytes()).hexdigest() == map_hash
report['status'] = 'saved_requires_runtime_verification'
(out / 'loot-priority-authoring.json').write_text(json.dumps(report, indent=2))
