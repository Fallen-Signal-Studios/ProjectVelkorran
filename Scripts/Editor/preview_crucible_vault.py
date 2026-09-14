"""Preview the taller upper enclosure without changing existing physical geometry."""
import json
from pathlib import Path
import unreal

root=Path(__file__).resolve().parents[2]
out=root/'Saved/Validation/Aurelion/CrucibleVault-20260913'; out.mkdir(parents=True,exist_ok=True)
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Aurelion_M12'
subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors=subsystem.get_all_level_actors(); by_label={a.get_actor_label():a for a in actors}
def snapshot():
    return {a.get_name():dict(label=a.get_actor_label(),transform=a.get_actor_transform().export_text(),components={c.get_name():dict(transform=c.get_world_transform().export_text(),collision=str(c.get_collision_enabled()),instances=[c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())] if isinstance(c,unreal.InstancedStaticMeshComponent) else None) for c in a.get_components_by_class(unreal.PrimitiveComponent)}) for a in actors}
before=snapshot()
baseline=out/'physical-baseline.json'
if not baseline.exists(): baseline.write_text(json.dumps(before,indent=2),encoding='utf8')
ceiling=by_label['Aurelion_Art_M12_Z08_89_b5e7cb']
components=ceiling.get_components_by_class(unreal.InstancedStaticMeshComponent)
assert len(components)==1 and components[0].get_instance_count()==216
c=components[0]; assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
for i in range(c.get_instance_count()):
    p=c.get_instance_transform(i,world_space=True).translation
    assert -3501<p.x<3501 and 18799<p.y<23201 and abs(p.z+495.37601)<.1
mesh=unreal.load_asset('/Game/Aurelion/Environment/Blender/SM_Aurelion_CrucibleVault'); assert mesh
with unreal.ScopedEditorTransaction('Raise Crucible visual enclosure'):
    c.set_visibility(False); c.set_hidden_in_game(True)
    label='ART_Crucible_UpperVault'
    actor=by_label.get(label)
    if actor: assert isinstance(actor,unreal.StaticMeshActor) and actor.static_mesh_component.static_mesh==mesh
    else: actor=subsystem.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(0,20800,-500))
    actor.set_actor_label(label); actor.set_folder_path('Aurelion/EnvironmentArt/Z08/UpperVault')
    actor.set_actor_location(unreal.Vector(0,20800,-500),False,False)
    actor.set_actor_rotation(unreal.Rotator(),False); actor.set_actor_scale3d(unreal.Vector(1,1,1))
    c=actor.static_mesh_component; c.set_static_mesh(mesh); c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    actor.set_actor_enable_collision(False)
    # High wall washes define the new height, avoiding additional floor glare.
    for side in (-1,1):
        for index,y in enumerate((19600,22000)):
            label='ENVL_CrucibleVault_'+str(side)+'_'+str(index)
            light=by_label.get(label)
            if light: assert isinstance(light,unreal.RectLight)
            else: light=subsystem.spawn_actor_from_class(unreal.RectLight,unreal.Vector(side*2300,y,300))
            light.set_actor_label(label); light.set_folder_path('Aurelion/00_Lighting')
            light.set_actor_location(unreal.Vector(side*2300,y,300),False,False)
            light.set_actor_rotation(unreal.Rotator(pitch=18,yaw=0 if side>0 else 180),False)
            c=light.get_component_by_class(unreal.RectLightComponent); c.set_mobility(unreal.ComponentMobility.MOVABLE)
            c.set_editor_property('intensity_units',unreal.LightUnits.LUMENS); c.set_intensity(2500)
            c.set_attenuation_radius(3000); c.set_source_width(1400); c.set_source_height(800)
            c.set_light_color(unreal.LinearColor(1,.94,.83,1))
    # The old art sign floats four metres ahead of the entry camera. Replace its
    # presentation with a destination label above the existing north exit.
    old_sign=by_label['Aurelion_Art_Sign_Z08_1cd400'].get_component_by_class(unreal.TextRenderComponent)
    old_sign.set_visibility(False); old_sign.set_hidden_in_game(True)
    label='ART_Crucible_WoundGallerySign'
    sign=by_label.get(label)
    if sign: assert isinstance(sign,unreal.TextRenderActor)
    else: sign=subsystem.spawn_actor_from_class(unreal.TextRenderActor,unreal.Vector(0,23160,-590))
    sign.set_actor_label(label); sign.set_folder_path('Aurelion/EnvironmentArt/Z08/UpperVault')
    sign.set_actor_location(unreal.Vector(0,23160,-590),False,False)
    sign.set_actor_rotation(unreal.Rotator(yaw=-90),False)
    c=sign.get_component_by_class(unreal.TextRenderComponent)
    c.set_text('WOUND GALLERY'); c.set_world_size(45)
    c.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
    c.set_text_render_color(unreal.Color(30,24,18,255))
    c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
after=snapshot()
assert before==after,'Existing geometry or collision changed during placement'
(out/'placement.json').write_text(json.dumps(dict(status='PREVIEW_UNSAVED',unchanged_existing_actors=len(json.loads(baseline.read_text())),hidden_ceiling_instances=216,visual_wall_height_metres=22,new_lights=4),indent=2),encoding='utf8')
texts=[]
for a in actors:
    p=a.get_actor_location()
    if 18000<p.y<23500:
        for c in a.get_components_by_class(unreal.TextRenderComponent):
            texts.append(dict(label=a.get_actor_label(),text=str(c.text),position=p.export_text(),world_size=c.world_size,hidden_in_game=c.get_editor_property('hidden_in_game'),visible=c.get_editor_property('visible')))
(out/'signs.json').write_text(json.dumps(texts,indent=2),encoding='utf8')
unreal.log('CRUCIBLE_VAULT_PREVIEW_UNSAVED')
