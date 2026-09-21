"""Restore companion traversal chooser parity with the corresponding player."""
from pathlib import Path
import hashlib,json,os,shutil,unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
maps=[root/'Content/Aurelion/Maps'/n for n in ('L_Aurelion_M12.umap','L_Aurelion_M13.umap')]
hashes=[hashlib.sha256(p.read_bytes()).hexdigest() for p in maps]
rows=[];prepared=[]
for hero in ('Selene','Tarrik'):
    player=unreal.load_asset('/Game/PlayerCharacters/BP_Sov'+hero)
    companion=unreal.load_asset('/Game/Aurelion/Characters/BP_Aurelion'+hero+'Companion')
    pcdo=unreal.get_default_object(player.generated_class());cdo=unreal.get_default_object(companion.generated_class())
    table=pcdo.get_editor_property('TraversalTable')
    assert table and table.get_path_name().endswith('CHT_TraversalAnims_Biped.CHT_TraversalAnims_Biped')
    assert cdo.get_editor_property('TraversalTable') is None,'Refuse to replace an existing chooser'
    appearances=[]
    for definition_path in ('/Game/Characters/Definitions/PD_'+hero,'/Game/Aurelion/Characters/NPC_Aurelion'+hero+'Companion'):
        definition=unreal.load_asset(definition_path)
        appearance=definition.get_editor_property('default_appearance');assert appearance
        attributes=appearance.get_editor_property('character_attributes')
        mesh=attributes.get_editor_property('base_mesh');assert mesh
        appearances.append(dict(appearance=appearance.get_path_name(),mesh=mesh.get_path_name(),
            skeleton=mesh.get_editor_property('skeleton').get_path_name(),
            anim_class=str(attributes.get_editor_property('base_mesh_anim_bp'))))
    assert appearances[0]['skeleton']==appearances[1]['skeleton']
    assert appearances[0]['anim_class']==appearances[1]['anim_class']
    prepared.append((hero,companion,cdo,table,appearances))
for hero,bp,cdo,table,appearances in prepared:
    disk=root/'Content/Aurelion/Characters'/('BP_Aurelion'+hero+'Companion.uasset')
    shutil.copy2(disk,out/disk.name)
    bp.modify();cdo.modify();cdo.set_editor_property('TraversalTable',table)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert unreal.get_default_object(bp.generated_class()).get_editor_property('TraversalTable')==table
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
    rows.append(dict(hero=hero,blueprint=bp.get_path_name(),table=table.get_path_name(),appearances=appearances,
        sha256=hashlib.sha256(disk.read_bytes()).hexdigest()))
assert [hashlib.sha256(p.read_bytes()).hexdigest() for p in maps]==hashes
(out/'companion-traversal-save.json').write_text(json.dumps(dict(status='saved_requires_fresh_readback',rows=rows,maps_unchanged=True),indent=2))
