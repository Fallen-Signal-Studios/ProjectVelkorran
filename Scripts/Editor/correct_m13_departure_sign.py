"""Correct the approach-facing faction arrows; preview by default, scoped save on request."""
from pathlib import Path
import hashlib, json, os, runpy, shutil
import unreal

root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
persist=bool(globals().get('SAVE_DEPARTURE_SIGN',False))
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Aurelion_M13' and not level.is_in_play_in_editor()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
by_name={a.get_actor_label():a for a in actors.get_all_level_actors()}
sign=by_name['Aurelion_Art_Sign_Z12_f79ad4']
component=sign.get_component_by_class(unreal.TextRenderComponent)
old='DEPARTURE CONCOURSE\n< DOMINION   REFORMATION >'
correct='DEPARTURE CONCOURSE\n< REFORMATION   DOMINION >'
assert str(component.text) in (old,correct),'Unexpected edited sign text; inspect before replacing'
original_text=str(component.text)
assert abs(sign.get_actor_rotation().yaw+90)<.01
right=unreal.MathLibrary.get_right_vector(unreal.Rotator(yaw=90))
positions={}
for faction,expected_side in [('Reformation',-1),('Dominion',1)]:
    for name in ('Z12_'+faction+'_Dock','ART_DepartureShuttle_'+faction):
        offset=by_name[name].get_actor_location()-sign.get_actor_location()
        projection=offset.x*right.x+offset.y*right.y+offset.z*right.z
        assert projection*expected_side>1000,(name,projection)
        positions[name]={'location':by_name[name].get_actor_location().export_text(),'view_right_cm':projection}
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
before=helpers['snapshot_actor_state'](actors.get_all_level_actors())
other_text={a.get_path_name():str(c.text) for a in actors.get_all_level_actors()
            if a!=sign and (c:=a.get_component_by_class(unreal.TextRenderComponent))}
table_path='/Game/Aurelion/Data/ST_AurelionText'
table_id=unreal.Name(table_path+'.ST_AurelionText')
key='Sign.DepartureConcourse'
table=unreal.load_asset(table_path);assert isinstance(table,unreal.StringTable)
entries={k:unreal.StringTableLibrary.get_table_entry_source_string(table_id,k)
         for k in unreal.StringTableLibrary.get_keys_from_string_table(table_id)}
assert key not in entries or entries[key]==correct
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
m12=root/'Content/Aurelion/Maps/L_Aurelion_M12.umap';m12_hash=digest(m12)
if persist:
    for p in [root/'Content/Aurelion/Maps/L_Aurelion_M13.umap',root/'Content/Aurelion/Data/ST_AurelionText.uasset']:
        shutil.copy2(p,out/(p.name+'.before'))
result=unreal.SovAurelionAuthoringLibrary.set_aurelion_strings(table,{key:correct})
assert result.get_editor_property('succeeded'),result.report
sign.modify();component.modify()
component.set_text(unreal.TextLibrary.text_from_string_table(table_id,key))

def verify():
    current=list(actors.get_all_level_actors())
    assert helpers['snapshot_actor_state'](current)==before
    target=next(a for a in current if a.get_actor_label()=='Aurelion_Art_Sign_Z12_f79ad4')
    text=target.get_component_by_class(unreal.TextRenderComponent).text
    assert str(text)==correct and unreal.TextLibrary.text_is_from_string_table(text)
    identity=unreal.TextLibrary.string_table_id_and_key_from_text(text)
    assert str(identity[0])==str(table_id) and identity[1]==key
    assert {a.get_path_name():str(c.text) for a in current if a!=target
            and (c:=a.get_component_by_class(unreal.TextRenderComponent))}==other_text
    expected=dict(entries);expected[key]=correct
    assert {k:unreal.StringTableLibrary.get_table_entry_source_string(table_id,k)
            for k in unreal.StringTableLibrary.get_keys_from_string_table(table_id)}==expected
    assert digest(m12)==m12_hash

verify()
if persist:
    assert unreal.EditorAssetLibrary.save_loaded_asset(table,only_if_is_dirty=False)
    assert level.save_current_level()
    assert level.load_level('/Game/Aurelion/Maps/L_Aurelion_M13')
    verify()
(out/'departure-sign-correction.json').write_text(json.dumps({
    'status':'saved_reloaded' if persist else 'unsaved_preview','before':original_text,'after':correct,
    'table':str(table_id),'key':key,'destinations':positions,'m12_unchanged':True,
    'transforms_collision_other_text_preserved':True,
    'qualification':'Direction checked against saved docks and shuttle positions; visual inspection still required.'
},indent=2))
runpy.run_path(str(root/'Scripts/Editor/preview_m13_route.py'),init_globals={
    'ALLOW_DIRTY_PREVIEW':not persist,'M13_ROUTE_VIEWS':[('departure-sign',(0,45900,160))]})
