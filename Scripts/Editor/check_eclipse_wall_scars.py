"""Verify fitted visual overlays without changing native room collision."""
from pathlib import Path
import json,unreal

def check_eclipse_wall_scars(actors):
    source=Path(unreal.Paths.project_dir())/'Art/Source/Aurelion/EclipseWallKit'
    fit=json.loads((source/'placement-fit.json').read_text());by_label={a.get_actor_label():a for a in actors};rows=[]
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    for original,placement in zip(fit['original'],fit['placements']):
        label,index,x,y,z,yaw=placement;a=by_label[label];c=a.get_component_by_class(unreal.StaticMeshComponent);mesh=c.static_mesh;t=a.get_actor_transform()
        assert c.get_path_name()==original['component'] and not a.get_actor_enable_collision()
        assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
        assert mesh.get_name()=='SM_Aurelion_KIT_EclipseWallScar_'+str(index)
        assert (t.translation-unreal.Vector(x,y,z)).length()<.001 and (t.scale3d-unreal.Vector(1,1,1)).length()<.00001
        r=t.rotation.rotator();assert abs(r.yaw-yaw)+abs(r.pitch)+abs(r.roll)<.001
        assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game') and not c.get_editor_property('override_materials')
        assert sm.get_num_uv_channels(mesh,0)==2 and sm.get_nanite_settings(mesh).get_editor_property('enabled')
        assert sm.get_nanite_settings(mesh).get_editor_property('position_precision')==10
        assert sm.get_simple_collision_count(mesh)==0 and sm.get_convex_collision_count(mesh)==0
        assert all('NativeDisplay' not in c.get_material(i).get_path_name() for i in range(c.get_num_materials()))
        rows.append(dict(actor=label,transform=t.export_text(),mesh=mesh.get_path_name()))
    assert len(actors)==3140
    return dict(overlays=rows,qualification='Four decorative replacements; full art quality and live campaign acceptance remain open.')
