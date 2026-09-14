"""Read-only request-prop inventory for the saved Z07 rescue room."""
from pathlib import Path
import json,os
import unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY']);editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());assert len(actors)==2923
rows=[]
for a in actors:
    p=a.get_actor_location();name=a.get_actor_label()
    if not (-1800<p.x<1800 and 13800<p.y<16200 and any(s in name.lower() for s in ('cage','resonator','isolated','priority'))):continue
    props={}
    for prop in ('request_id','mission_id','beat_id','operation','story','handoff_anchor','co_action_anchor','thermal','thermal_director','thermal_participant_id','destination_mission','retry_objective','retry_director','action_text'):
        try:
            v=a.get_editor_property(prop);props[prop]=v.get_path_name() if isinstance(v,unreal.Object) else str(v)
        except Exception:pass
    components=[]
    for c in a.get_components_by_class(unreal.SceneComponent):
        row=dict(name=c.get_name(),class_name=c.get_class().get_name(),relative=c.get_relative_transform().export_text(),world=c.get_world_transform().export_text(),parent=c.get_attach_parent().get_name() if c.get_attach_parent() else None)
        if isinstance(c,unreal.PrimitiveComponent):row.update(collision=str(c.get_collision_enabled()),profile=str(c.get_collision_profile_name()),visible=c.get_editor_property('visible'),hidden=c.get_editor_property('hidden_in_game'))
        if isinstance(c,unreal.BoxComponent):row['extent']=[c.get_scaled_box_extent().x,c.get_scaled_box_extent().y,c.get_scaled_box_extent().z]
        if isinstance(c,unreal.StaticMeshComponent) and c.static_mesh:
            b=c.static_mesh.get_bounds();row.update(mesh=c.static_mesh.get_path_name(),mesh_origin=[b.origin.x,b.origin.y,b.origin.z],mesh_extent=[b.box_extent.x,b.box_extent.y,b.box_extent.z],materials=[c.get_material(i).get_path_name() if c.get_material(i) else None for i in range(c.get_num_materials())])
        components.append(row)
    rows.append(dict(actor=name,class_name=a.get_class().get_name(),transform=a.get_actor_transform().export_text(),properties=props,components=components))
assert rows
(out/'z07-request-props.json').write_text(json.dumps(dict(status='read_only',actors=rows,qualification='Saved editor inventory; no interaction invoked or state changed.'),indent=2))
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
print('Z07_REQUEST_PROP_SURVEY_PASS',len(rows))
gates=[]
for a in actors:
    if 'JournalGate' not in a.get_class().get_name():continue
    beat=str(a.get_editor_property('beat_id'))
    if beat not in ('DestroyDominionResonator','DestroyReformationCage'):continue
    gates.append(dict(actor=a.get_actor_label(),class_name=a.get_class().get_name(),beat=beat,mission=str(a.get_editor_property('mission_id')),targets=[x.get_actor_label() for x in a.get_editor_property('bound_visual_actors')],use_body=a.get_editor_property('use_gate_body'),block_after=a.get_editor_property('block_after_completion')))
(out/'z07-capture-gates.json').write_text(json.dumps(dict(gates=gates,expected={f:[a.get_actor_label() for a in actors if a.get_actor_label().startswith('Z07_Cage_') and '_'+f in a.get_actor_label()] for f in ('DOMINION','REFORMATION')}),indent=2))
