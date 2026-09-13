"""Import the explicit Downloads SFX shortlist without replacing live cues."""
import hashlib
import json
import os
from pathlib import Path
import traceback
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
downloads = Path.home() / 'Downloads'
manifest = Path(unreal.Paths.project_dir()).resolve() / 'Scripts/Editor/Manifests/DownloadsSFX-2026-09-13.json'
selection = json.loads(manifest.read_text(encoding='utf-8'))
report = dict(status='preflight', imported=[], runtime_cues_changed=False, errors=[])
try:
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
    tasks = []
    for entry in selection['sounds']:
        source = downloads / entry['source']
        assert source.is_file() and hashlib.sha256(source.read_bytes()).hexdigest() == entry['sha256'], str(source)
        destination = selection['destination'] + '/' + entry['category']
        asset_path = destination + '/' + entry['asset_name']
        assert not unreal.EditorAssetLibrary.does_asset_exist(asset_path), 'Preserve existing asset: ' + asset_path
        task = unreal.AssetImportTask()
        for key, value in dict(filename=str(source), destination_path=destination,
                               destination_name=entry['asset_name'], automated=True,
                               replace_existing=False, save=True).items():
            task.set_editor_property(key, value)
        tasks.append(task)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    for entry, task in zip(selection['sounds'], tasks):
        paths = list(task.get_editor_property('imported_object_paths'))
        assert len(paths) == 1, (entry['source'], paths)
        sound = unreal.load_asset(paths[0])
        assert isinstance(sound, unreal.SoundWave), paths
        duration = float(sound.get_editor_property('duration'))
        channels = int(sound.get_editor_property('num_channels'))
        assert duration > 0 and channels in (1, 2), (paths, duration, channels)
        assert abs(duration - entry['duration_seconds']) < 0.05, paths
        report['imported'].append(dict(asset=paths[0], duration_seconds=duration,
                                       channels=channels, source_sha256=entry['sha256']))
    report['status'] = 'imported and verified; audition/mix pending'
except Exception:
    report.update(status='failed')
    report['errors'].append(traceback.format_exc())
    raise
finally:
    (out/'downloads-sfx-import.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
