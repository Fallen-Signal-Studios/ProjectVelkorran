"""Use each protagonist's authored mesh frame on its mission companion proxy."""
import json
import os
from pathlib import Path
import shutil
import unreal

FIELDS = ('relative_location', 'relative_rotation', 'relative_scale3d')

def mesh_of(cdo):
    meshes = [m for m in cdo.get_components_by_class(unreal.SkeletalMeshComponent)
              if m.get_name() == 'CharacterMesh0']
    assert len(meshes) == 1
    return meshes[0]

def align(hero, output):
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
    player = unreal.load_asset('/Game/PlayerCharacters/BP_Sov' + hero)
    proxy = unreal.load_asset('/Game/Aurelion/Characters/BP_Aurelion' + hero + 'Companion')
    source = mesh_of(unreal.get_default_object(player.generated_class()))
    target = mesh_of(unreal.get_default_object(proxy.generated_class()))
    expected = {key: source.get_editor_property(key).export_text() for key in FIELDS}
    before = {key: target.get_editor_property(key).export_text() for key in FIELDS}
    changed = any(source.get_editor_property(key) != target.get_editor_property(key) for key in FIELDS)
    if changed:
        output = Path(output)
        output.mkdir(parents=True, exist_ok=True)
        package = Path(unreal.Paths.project_dir()) / ('Content/Aurelion/Characters/BP_Aurelion' + hero + 'Companion.uasset')
        backup = output / package.name
        assert not backup.exists(), 'Preserve the first backup'
        shutil.copy2(package, backup)
        proxy.modify()
        target.modify()
        for key in FIELDS:
            target.set_editor_property(key, source.get_editor_property(key))
        unreal.BlueprintEditorLibrary.compile_blueprint(proxy)
        target = mesh_of(unreal.get_default_object(proxy.generated_class()))
        assert all(target.get_editor_property(key) == source.get_editor_property(key) for key in FIELDS)
        assert unreal.EditorAssetLibrary.save_loaded_asset(proxy, only_if_is_dirty=False)
    return dict(hero=hero, changed=changed, before=before, after=expected)

if __name__ == '__main__':
    output = Path(os.environ['SOV_AURELION_RUN_DIRECTORY']) / 'mesh-frame-repair'
    output.mkdir(exist_ok=False)
    rows = [align(hero, output) for hero in ('Selene', 'Tarrik')]
    (output / 'result.json').write_text(json.dumps(dict(status='saved_requires_runtime_verification', rows=rows), indent=2))
