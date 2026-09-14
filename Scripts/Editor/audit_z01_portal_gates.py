"""Read-only end-wall/gate fit evidence. Does not apply journal state."""
import json
import os
from pathlib import Path
import unreal

out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not editor.is_in_play_in_editor() and not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
labels={a.get_actor_label():a for a in actors}; rows=[]
for label in ('Aurelion_TarrikArrivalGate','Aurelion_PressureHallExit','aureliondoors','Z01_Lintel_S','Z01_Lintel_N','Z01__UpperSpan_01','Z01__UpperSpan_02'):
    a=labels[label]; components=[]
    for c in a.get_components_by_class(unreal.PrimitiveComponent):
        row=dict(name=c.get_name(),kind=c.get_class().get_name(),transform=c.get_world_transform().export_text(),
                 collision=str(c.get_collision_enabled()),profile=str(c.get_collision_profile_name()),
                 visible=c.is_visible(),bounds=[v.export_text() if hasattr(v,'export_text') else v for v in unreal.SystemLibrary.get_component_bounds(c)])
        if isinstance(c,unreal.StaticMeshComponent):
            row['mesh']=c.static_mesh.get_path_name() if c.static_mesh else None
        if isinstance(c,unreal.BoxComponent): row['unscaled_extent']=c.get_unscaled_box_extent().export_text()
        components.append(row)
    row=dict(label=label,transform=a.get_actor_transform().export_text(),components=components)
    if isinstance(a,unreal.SovAurelionJournalGate):
        row['contract']={k:str(a.get_editor_property(k)) for k in ('mission_id','beat_id','block_after_completion','use_gate_body')}
        row['bound_visual_actors']=[v.get_actor_label() for v in a.get_editor_property('bound_visual_actors')]
    rows.append(row)
(out/'portal-gates.json').write_text(json.dumps(rows,indent=2))
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
