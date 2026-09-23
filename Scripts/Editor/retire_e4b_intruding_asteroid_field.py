"""Hide the one decorative asteroid field intruding into the E4B play space."""
import hashlib
import json
import os
from pathlib import Path
import shutil

import unreal


project = Path(unreal.Paths.project_dir())
map_file = project / 'Content/Aurelion/Maps/L_Aurelion_M12.umap'
expected = 'BE71EC7724DE609809F880DA147FD7920DCEC6AD03AE9F2F0E306D1AD1BBAC53'
before_hash = hashlib.sha256(map_file.read_bytes()).hexdigest().upper()
assert before_hash == expected, f'M12 changed since obstruction inventory: {before_hash}'
world = unreal.EditorLevelLibrary.get_editor_world()
assert world and world.get_name() == 'L_Aurelion_M12'
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()

actors = [actor for actor in unreal.EditorLevelLibrary.get_all_level_actors()
          if actor.get_actor_label() == 'BP_AsteroidField_Globular2']
assert len(actors) == 1
actor = actors[0]
assert actor.get_class().get_path_name().endswith(
    'BP_AsteroidField_Globular.BP_AsteroidField_Globular_C')
position = actor.get_actor_location()
assert (abs(position.x) < 1 and abs(position.y - 89350) < 1
        and abs(position.z - 20630) < 1)
assert not actor.get_editor_property('hidden') and actor.get_actor_enable_collision()

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
backup = out / 'L_Aurelion_M12.before-asteroid-retire.umap'
assert not backup.exists()
shutil.copy2(map_file, backup)
assert hashlib.sha256(backup.read_bytes()).hexdigest().upper() == expected

actor.set_actor_hidden_in_game(True)
actor.set_actor_enable_collision(False)
assert actor.get_editor_property('hidden') and not actor.get_actor_enable_collision()
assert unreal.EditorLevelLibrary.save_current_level()

after_hash = hashlib.sha256(map_file.read_bytes()).hexdigest().upper()
assert after_hash != before_hash
report = dict(status='passed', map=str(map_file), actor=actor.get_path_name(),
              label=actor.get_actor_label(), location=position.export_text(),
              before_hash=before_hash, after_hash=after_hash,
              backup=str(backup), hidden_in_game=actor.get_editor_property('hidden'),
              collision_enabled=actor.get_actor_enable_collision())
(out / 'retired-e4b-asteroid-field.json').write_text(json.dumps(report, indent=2),
                                                    encoding='utf-8')
