"""Fitted custom end walls and journal door visuals; preview unless explicitly saved."""
import json
import os
from pathlib import Path
import runpy
import shutil
import time
import unreal
persist=bool(globals().get('PERSIST',False))
root=Path(unreal.Paths.project_dir()).resolve(); out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
helpers=runpy.run_path(str(root/'Scripts/Editor/aurelion_architecture_helpers.py'))
checks=runpy.run_path(str(root/'Scripts/Editor/check_z01_endwalls.py'))
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(); assert world.get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
original=list(subsystem.get_all_level_actors()); by_label={a.get_actor_label():a for a in original}
text_rows=[]
for a in original:
    for text in a.get_components_by_class(unreal.TextRenderComponent):
        p=text.get_world_location()
        if -18000<p.y<-11500 and -8400<p.x<-5500:
            text_rows.append(dict(actor=a.get_actor_label(),component=text.get_name(),text=str(text.get_editor_property('text')),position=p.export_text(),visible=text.is_visible()))
(out/'legacy-text-audit.json').write_text(json.dumps(text_rows,indent=2))
assert len(original)==1915 and not any(a.get_actor_label().startswith('KIT_Z01_Endwall_') for a in original)
before=helpers['snapshot_actor_state'](original)
source=root/'Art/Source/Aurelion/EndwallKit'; destination='/Game/Aurelion/Environment/ArchitectureKit'
materials={key:destination+'/Materials/M_AurelionKit_'+value for key,value in {'M_Aurelion_IvoryStone':'Ivory','M_Aurelion_ChannelShadow':'Reveal','M_Aurelion_AncientGold':'Gold'}.items()}
meshes={spec['asset']:helpers['import_owned_mesh'](spec,source,destination+'/Meshes',materials) for spec in json.loads((source/'manifest.json').read_text())['modules']}
meshes['SM_Aurelion_KIT_WallPlain_4x7']=unreal.load_asset(destination+'/Meshes/SM_Aurelion_KIT_WallPlain_4x7')
placed=[]
for side,y,yaw,gate_label in (('South',-17500,0,'Aurelion_TarrikArrivalGate'),('North',-11900,180,'Aurelion_PressureHallExit')):
    layout=[('Portal',0,'Portal_6x4p5')]+[(f'Bay_{i}',x,'WallPlain_4x7') for i,x in enumerate((-1000,-600,600,1000))]+[(f'Return_{i}',x,'EndReturn_1x7') for i,x in enumerate((-1250,1250))]
    for name,x,suffix in layout:
        a=subsystem.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(-7000+x,y,0),unreal.Rotator(yaw=yaw))
        label=f'KIT_Z01_Endwall_{side}_{name}'; a.set_actor_label(label); a.set_folder_path('Aurelion/CustomArchitecture/Z01/EndWalls')
        a.static_mesh_component.set_static_mesh(meshes['SM_Aurelion_KIT_'+suffix]); a.static_mesh_component.set_collision_profile_name('NoCollision')
        a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION); a.set_actor_enable_collision(False)
        placed.append(dict(label=label,transform=a.get_actor_transform().export_text(),mesh=a.static_mesh_component.static_mesh.get_path_name()))
    gate=by_label[gate_label]; authority=checks['gate_authority'](gate); c=gate.visual
    c.modify(); c.set_static_mesh(meshes['SM_Aurelion_KIT_JournalDoor_6x4p5']); c.set_editor_property('override_materials',[])
    # Actor/root scales belong to the existing gate body. Only invert them on its visual child.
    p=gate.get_actor_location(); s=gate.get_actor_scale3d()
    c.set_relative_location(unreal.Vector((-7000-p.x)/s.x,(y-p.y)/s.y,-p.z/s.z),False,False)
    c.set_relative_rotation(unreal.Rotator(yaw=yaw),False,False); c.set_relative_scale3d(unreal.Vector(1/s.x,1/s.y,1/s.z))
    assert checks['gate_authority'](gate)==authority,'Gate body or journal authority changed'
for label in ('aureliondoors','Z01__UpperSpan_01','Z01__UpperSpan_02'):
    c=by_label[label].static_mesh_component; c.modify(); c.set_visibility(False,False); c.set_hidden_in_game(True,False)
legacy_sign=by_label['Aurelion_Art_Sign_Z01_828f4d'].get_component_by_class(unreal.TextRenderComponent)
assert str(legacy_sign.get_editor_property('text'))=='PRESSURE HALL\nAHEAD: SURVIVOR BEND'
legacy_sign.modify(); legacy_sign.set_visibility(False,False); legacy_sign.set_hidden_in_game(True,False)
assert helpers['snapshot_actor_state'](original)==before,'Existing actor transform/collision changed'
geometry=checks['check_endwalls'](world,list(subsystem.get_all_level_actors()))
if persist:
    shutil.copy2(root/'Content/Aurelion/Maps/L_Aurelion_M12.umap',out/'L_Aurelion_M12-before.umap'); assert editor.save_current_level()
(out/'endwall-fit.json').write_text(json.dumps(dict(status='saved' if persist else 'unsaved_preview',placements=placed,geometry=geometry,
    changes='14 fitted masonry actors; two journal visual meshes/transforms; three legacy meshes and the floating Z01 art sign hidden. All original actor transforms and collision retained.',
    qualification='Editor authoring only. Existing gate visibility toggles, oversized bodies and live mission traversal remain unqualified.'),indent=2))
editor.editor_set_game_view(True)
capture=(root/'Scripts/Editor/fit_z01_lower_architecture.py').read_text(encoding='utf-8-sig').split("p=by_label['Z01_Entry_StandIn']",1)[1]
capture=capture.replace("('entry',unreal.Vector(p.x,p.y,p.z+165),unreal.Rotator(yaw=90),90)","('north-portal',unreal.Vector(-7180,-13250,165),unreal.Rotator(pitch=12,yaw=82),80)")
capture=capture.replace("('west-wall',unreal.Vector(-7600,-14900,165),unreal.Rotator(pitch=12,yaw=180),75)","('south-portal',unreal.Vector(-7200,-16250,165),unreal.Rotator(pitch=12,yaw=-82),85)")
exec(compile("p=by_label['Z01_Entry_StandIn']"+capture,'z01_endwall_capture','exec'))
