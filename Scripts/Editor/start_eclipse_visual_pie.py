"""Fresh-profile PIE startup for visual inspection, with ordinary default accessibility setup."""
from pathlib import Path
import runpy
import unreal
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor()
saved=Path(unreal.Paths.project_saved_dir()).resolve()
validation=Path(unreal.Paths.project_dir()).resolve()/'Saved/Validation/Aurelion'
assert saved.is_relative_to(validation), 'Requires isolated Aurelion validation profile'
assert not list((saved/'SaveGames').glob('*.sav')), 'Expected fresh profile'
settings=unreal.GameUserSettings.get_game_user_settings()
assert settings.complete_accessibility_setup()
runpy.run_path(str(Path(unreal.Paths.project_dir()).resolve()/'Scripts/Editor/inspect_eclipse_visual_pie.py'))
editor.editor_request_begin_play()
