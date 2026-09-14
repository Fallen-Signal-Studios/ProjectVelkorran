"""Replace the upper visual shell with the authored coffer assembly."""
from pathlib import Path
import json,os,runpy,shutil,time
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);original=list(subsystem.get_all_level_actors());by_label={a.get_actor_label():a for a in original};assert len(original)==3140
source=root/'Art/Source/Aurelion/Z08VaultKit';helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
bands=[by_label['Z08__BlackLowerBand_'+str(i).zfill(2)] for i in (1,2)]
unchanged=[a for a in original if a not in bands];before=helpers['snapshot_actor_state'](unchanged)
def row(a):
    c=a.static_mesh_component
    return dict(actor=a.get_actor_label(),transform=a.get_actor_transform().export_text(),mesh=c.static_mesh.get_path_name(),actor_collision=a.get_actor_enable_collision(),collision=str(c.get_collision_enabled()))
vault=by_label['ART_Crucible_UpperVault'];assert vault.static_mesh_component.static_mesh.get_name()=='SM_Aurelion_CrucibleVault'
fit=dict(vault_transform=vault.get_actor_transform().export_text(),bands=[row(a) for a in bands],physical_walls=[row(a) for name,a in by_label.items() if name.startswith(('Z08_Wall_','Z08_Lintel_'))])
assert len(fit['physical_walls'])==8
dest='/Game/Aurelion/Environment/ArchitectureKit';materials={k:dest+'/Materials/M_AurelionKit_'+v for k,v in {'M_Aurelion_IvoryStone':'PavingIvory','M_Aurelion_AncientGold':'Gold','M_Aurelion_ChannelShadow':'Reveal'}.items()}
materials['M_Vault_Inlay']='/Game/Aurelion/Environment/Blender/M_RecessPanel_WarmInlay'
if globals().get('PERSIST',False):
    inlay=unreal.load_asset(materials['M_Vault_Inlay']);assert isinstance(inlay,unreal.Material)
    shutil.copy2(root/'Content/Aurelion/Environment/Blender/M_RecessPanel_WarmInlay.uasset',out/'M_RecessPanel_WarmInlay-before.uasset')
    inlay.modify();inlay.set_editor_property('used_with_nanite',True);unreal.MaterialEditingLibrary.recompile_material(inlay)
    assert unreal.EditorAssetLibrary.save_loaded_asset(inlay)
meshes={spec['asset']:helpers['import_owned_mesh'](spec,source,dest+'/Meshes',materials) for spec in json.loads((source/'manifest.json').read_text())['modules']}
c=vault.static_mesh_component;c.modify();c.set_static_mesh(meshes['SM_Aurelion_KIT_Z08VaultAssembly']);c.set_editor_property('override_materials',[])
fit['materials']=[c.get_material(i).get_path_name() for i in range(c.get_num_materials())]
for a in bands:
    c=a.static_mesh_component;c.modify();c.set_visibility(False,False);c.set_hidden_in_game(True,False);c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);a.set_actor_enable_collision(False)
assert helpers['snapshot_actor_state'](unchanged)==before
(source/'vault-fit.json').write_text(json.dumps(fit,indent=2))
geometry=runpy.run_path(str(root/'Scripts/Editor/check_z08_vault.py'))['check_z08_vault'](world,list(subsystem.get_all_level_actors()))
persist=bool(globals().get('PERSIST',False))
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'z08-vault-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',geometry=geometry,preserved_actor_states=len(unchanged)),indent=2))
if not globals().get('SKIP_VAULT_CAPTURE',False):
    editor.editor_set_game_view(True)
    capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
    capture=capture.replace("('entry',unreal.Vector(p.x,p.y,p.z+165),unreal.Rotator(yaw=90),90)","('vault-entry',unreal.Vector(-500,19800,-950),unreal.Rotator(pitch=20,yaw=75),90)")
    capture=capture.replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('vault-soffit',unreal.Vector(0,21400,-250),unreal.Rotator(pitch=55,yaw=-90),90)").replace('z01-','z08-')
    exec(compile("p=by_label['Z08_Entry_StandIn']"+capture,'z08_vault_review','exec'))
