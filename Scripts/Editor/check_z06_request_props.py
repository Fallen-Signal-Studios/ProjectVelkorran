"""Custom request lecterns retain the authoritative request and collision owners."""
from pathlib import Path
import json
import unreal

def check_z06_request_props(world,actors):
    root=Path(unreal.Paths.project_dir());baseline=json.loads((root/'Art/Source/Aurelion/RequestConsoleKit/request-baseline.json').read_text());labels={a.get_actor_label():a for a in actors};rows=[]
    owners=[labels[row['actor']] for row in baseline['actors']];sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    for row in baseline['actors']:
        a=labels[row['actor']];retry='RetryEncounter' in row['actor'];v=a.visual;b=a.body
        assert a.get_actor_transform().export_text()==row['transform']
        for key,expected in row['properties'].items():
            value=a.get_editor_property(key);value=value.get_path_name() if isinstance(value,unreal.Object) else str(value)
            assert value==expected,(row['actor'],key)
        components={c.get_name():c for c in a.get_components_by_class(unreal.SceneComponent)}
        for old in row['components']:
            if old['name']=='Visual':continue
            c=components[old['name']]
            assert c.get_world_transform().export_text()==old['world']
            if old['name']!='StaticMesh':assert c.get_relative_transform().export_text()==old['relative']
            if 'collision' in old:assert str(c.get_collision_enabled())==old['collision'] and str(c.get_collision_profile_name())==old['profile']
            if old['name']=='StaticMesh':assert not c.get_editor_property('visible') and c.get_editor_property('hidden_in_game')
        assert (b.get_scaled_box_extent()-unreal.Vector(25,35,45)).length()<.001 and v.get_attach_parent()==b
        t=v.get_relative_transform();assert (t.translation-unreal.Vector(0,12.068254,0)).length()<.001 and (t.scale3d-unreal.Vector(1,1,1)).length()<.001
        rotation=v.get_editor_property('relative_rotation');assert abs(abs(rotation.yaw)-(180 if retry else 0))<.001 and abs(rotation.pitch)+abs(rotation.roll)<.001
        m=v.static_mesh;assert m.get_name()=='SM_Aurelion_KIT_RequestConsole'+('' if retry else 'RaisedBase')
        assert v.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and v.get_editor_property('visible') and not v.get_editor_property('hidden_in_game')
        assert sm.get_num_uv_channels(m,0)==2 and sm.get_nanite_settings(m).get_editor_property('enabled') and sm.get_simple_collision_count(m)==0
        origin,extent=v.get_local_bounds();bottom=unreal.MathLibrary.transform_location(v.get_world_transform(),unreal.Vector(0,0,origin.z))
        raw=unreal.SystemLibrary.line_trace_single_by_profile(world,bottom+unreal.Vector(0,0,5),bottom-unreal.Vector(0,0,10),'Pawn',False,owners,unreal.DrawDebugTrace.NONE,True)
        h=next(x for x in raw if isinstance(x,unreal.HitResult)) if isinstance(raw,tuple) else raw
        assert h and h.to_tuple()[0] and abs(h.to_tuple()[5].z-bottom.z)<.1
        rows.append(dict(actor=row['actor'],mesh=m.get_name(),bottom_cm=bottom.z,floor_cm=h.to_tuple()[5].z))
    return dict(actors=rows,qualification='Saved visual, grounding and unchanged request bindings/body; native interaction and final art require live acceptance.')
