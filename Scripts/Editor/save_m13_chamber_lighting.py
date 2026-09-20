"""Apply the reviewed chamber uplights, save M13 only, and verify after reload."""
from pathlib import Path
import hashlib
import json
import os
import runpy
import shutil
import unreal

root = Path(unreal.Paths.project_dir())
out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not level.is_in_play_in_editor()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name() == 'L_Aurelion_M13'
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
map_file = root / 'Content/Aurelion/Maps/L_Aurelion_M13.umap'
m12 = root / 'Content/Aurelion/Maps/L_Aurelion_M12.umap'
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
m12_before = digest(m12)
shutil.copy2(map_file, out / 'L_Aurelion_M13.before.umap')
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
before = {a.get_actor_label(): a.get_class().get_name() for a in actors.get_all_level_actors()}
module = runpy.run_path(str(root / 'Scripts/Editor/refine_m13_chamber_lighting.py'))
expected = module['apply']()
assert level.save_current_level()
assert level.load_level('/Game/Aurelion/Maps/L_Aurelion_M13')
saved = {a.get_actor_label(): a for a in actors.get_all_level_actors()}
for name, class_name in before.items():
    assert name in saved and saved[name].get_class().get_name() == class_name
assert set(saved) - set(before) == set(expected) - set(before)
for name in expected:
    c = saved[name].get_component_by_class(unreal.SpotLightComponent)
    assert c and abs(c.intensity - 2000) < .01
    assert abs(c.attenuation_radius - 4500) < .01
    assert c.get_editor_property('cast_shadows')
assert digest(m12) == m12_before
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
(out / 'saved-lighting.json').write_text(json.dumps(dict(
    status='PASS', lights=expected, reloaded=True, m12_unchanged=True,
    existing_actor_names_and_classes_preserved=True,
    qualification='Saved M13 lighting and reload verification; GPU cost and full player-route acceptance remain open.'
), indent=2))
