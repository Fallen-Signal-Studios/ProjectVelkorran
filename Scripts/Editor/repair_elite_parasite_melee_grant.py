"""Restore the Elite's authored parasite melee without replacing its boss loadout."""
import json
import os
import shutil
from pathlib import Path
import unreal

out = Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
root = '/Game/Aurelion/Enemies/'
report = dict(status='failed', saved=[], runtime_qualified=False)
try:
    assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
    npc = unreal.load_asset(root + 'NPC_AurelionElite')
    config = npc.get_editor_property('ability_configuration')
    assert config.get_path_name() == root + 'AC_Abilities_AurelionElite.AC_Abilities_AurelionElite'
    replacement = unreal.load_asset(root + 'Abilities/GA_EclipseElite_Melee').generated_class()
    cdo = unreal.get_default_object(replacement)
    data = cdo.get_editor_property('attack_definition')
    assert data.validate() == ''
    nodes = list(data.get_editor_property('nodes'))
    mesh = unreal.load_asset('/Game/Parasites_Pack/Mesh/SK_Fat')
    skeleton = mesh.get_editor_property('skeleton')
    bones = {str(n) for n in unreal.SovEclipseAnimationAuthoringLibrary.mesh_bones(mesh)}
    assert nodes and cdo.get_editor_property('allow_unarmed')
    for node in nodes:
        assert node.montage and node.montage.get_editor_property('skeleton') == skeleton
        assert str(node.start_socket) in bones and str(node.end_socket) in bones
    before = list(config.get_editor_property('default_abilities'))
    effects = list(config.get_editor_property('startup_effects'))
    old = [a for a in before if a.get_name() == 'GA_Melee_Punch_Unarmed_C']
    assert len(old) == 1 and replacement not in before, 'Expected the exact regressed grant once'
    bosses = (unreal.SovGameplayAbility_AurelionEliteSlam.static_class(),
              unreal.SovGameplayAbility_AurelionEliteLance.static_class(),
              unreal.SovGameplayAbility_AurelionEliteSummon.static_class())
    assert all(before.count(a) == 1 for a in bosses)
    expected = [replacement if a == old[0] else a for a in before]
    disk = Path(unreal.Paths.project_dir()).resolve() / 'Content/Aurelion/Enemies/AC_Abilities_AurelionElite.uasset'
    backup = out / disk.name
    assert not backup.exists()
    shutil.copy2(disk, backup)
    report.update(before=[a.get_path_name() for a in before],
                  expected=[a.get_path_name() for a in expected],
                  startup_effects=[a.get_path_name() for a in effects],
                  montages=[n.montage.get_path_name() for n in nodes])
    config.set_editor_property('default_abilities', expected)
    assert list(config.get_editor_property('default_abilities')) == expected
    assert list(config.get_editor_property('startup_effects')) == effects
    assert unreal.EditorAssetLibrary.save_loaded_asset(config, only_if_is_dirty=False)
    report.update(status='saved_requires_fresh_readback', saved=[config.get_path_name()])
finally:
    (out / 'elite-melee-repair.json').write_text(json.dumps(report, indent=2))
