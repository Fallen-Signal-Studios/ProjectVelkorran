"""Retire five oversized graybox objective cards from the player-facing M12 map."""
import hashlib
import json
import os
import shutil
from pathlib import Path

import unreal


root = Path(unreal.Paths.project_dir())
map_file = root / 'Content/Aurelion/Maps/L_Aurelion_M12.umap'
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
before = os.environ.get('SOV_M12_EXPECTED_SHA',
                        'F60912F6C87926FC2EC7D9CE4DD34BEC4CC69CE486FA22AC6C7062F9F3690CBE')
expected = {
    'TextRenderActor_116': 'Pressure hall\nBreak the lead drone’s formation.',
    'TextRenderActor_117': 'Relay overlook\nDisable both receivers and clear the hostiles.',
    'TextRenderActor_118': 'Shared breach\nOpen the blue rescue approach and protect the survivors.',
    'TextRenderActor_119': 'Quarantine crucible\nIsolate both Weaver links. Keep the elite alive.',
    'TextRenderActor_120': 'Thermal Fracture\nPosition Selene, arrest the joint, then confirm nearby.',
}
sha = lambda: hashlib.sha256(map_file.read_bytes()).hexdigest().upper()
assert sha() == before, 'M12 map changed since the read-only inventory; do not overwrite it'
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors = {actor.get_name(): actor for actor in unreal.EditorLevelLibrary.get_all_level_actors()}
components = {}
for name, text in expected.items():
    actor = actors.get(name)
    assert actor and actor.get_class() == unreal.TextRenderActor.static_class(), name
    found = actor.get_components_by_class(unreal.TextRenderComponent)
    assert len(found) == 1 and str(found[0].get_editor_property('text')) == text, name
    assert not found[0].get_editor_property('hidden_in_game'), name
    components[name] = found[0]
shutil.copy2(map_file, out / 'L_Aurelion_M12-before.umap')
for component in components.values():
    component.set_editor_property('hidden_in_game', True)
assert sha() == before, 'M12 map changed concurrently before save'
assert level.save_current_level()
assert all(component.get_editor_property('hidden_in_game') for component in components.values())
(out / 'retired-world-objective-cards.json').write_text(json.dumps(dict(
    status='passed', map_before_sha256=before, map_after_sha256=sha(),
    hidden_actor_names=list(components)), indent=2), encoding='utf-8')
