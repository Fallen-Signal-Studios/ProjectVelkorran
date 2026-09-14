"""Controlled local key-color comparison; no renderer or exposure changes."""
from pathlib import Path
import json,os,runpy,shutil,time
import unreal
root=Path(unreal.Paths.project_dir());out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);original=list(subsystem.get_all_level_actors());by_label={a.get_actor_label():a for a in original};assert len(original)==2843
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'));before=helpers['snapshot_actor_state'](original);rows=[]
for name in ('ENVL_Z06_Key_00','ENVL_Z06_Key_02'):
    c=by_label[name].get_component_by_class(unreal.RectLightComponent);color=c.get_editor_property('light_color')
    old=[color.r,color.g,color.b,color.a];assert old==[255,161,87,255]
    names=('intensity','attenuation_radius','source_width','source_height','cast_shadows','indirect_lighting_intensity','use_temperature','temperature','visible')
    properties={key:str(c.get_editor_property(key)) for key in names}
    c.modify();c.set_light_color(unreal.LinearColor(.92,.85,.70,1))
    color=c.get_editor_property('light_color');new=[color.r,color.g,color.b,color.a];assert new==[246,237,218,255]
    assert {key:str(c.get_editor_property(key)) for key in names}==properties
    rows.append(dict(actor=name,before_rgb8=old,after_rgb8=new,properties=properties,transform=c.get_world_transform().export_text()))
assert helpers['snapshot_actor_state'](original)==before
persist=bool(globals().get('SAVE_KEY_BALANCE',False))
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap');assert editor.save_current_level()
(out/'key-balance-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',lights=rows,qualification='Only two local light colors changed; live gameplay and final lighting acceptance remain open.'),indent=2))
if not globals().get('SKIP_KEY_CAPTURE',False):
    editor.editor_set_game_view(True)
    capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
    capture=capture.replace("('entry',unreal.Vector(p.x,p.y,p.z+165),unreal.Rotator(yaw=90),90)","('climb-face',unreal.Vector(-850,9150,-340),unreal.Rotator(pitch=-10,yaw=150),65)").replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('climb-landing',unreal.Vector(-700,9870,-100),unreal.Rotator(pitch=-20,yaw=-155),65)").replace('z01-','z06-')
    if globals().get('KEY_BALANCE_WIDE_CAPTURE',False):
        capture=capture.replace("('climb-face',unreal.Vector(-850,9150,-340),unreal.Rotator(pitch=-10,yaw=150),65)","('housing-front',unreal.Vector(1100,7550,-330),unreal.Rotator(pitch=6,yaw=90),80)").replace("('climb-landing',unreal.Vector(-700,9870,-100),unreal.Rotator(pitch=-20,yaw=-155),65)","('housing-rear',unreal.Vector(400,9600,-120),unreal.Rotator(pitch=-5,yaw=-60),85)")
    exec(compile("p=by_label['Z06_Entry_StandIn']"+capture,'key_balance_capture','exec'),globals())
