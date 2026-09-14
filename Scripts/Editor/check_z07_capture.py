"""Capture frame fit, native collision, and faction-specific journal targets."""
from pathlib import Path
import json
import unreal
def check_z07_capture(world,actors):
    labels={a.get_actor_label():a for a in actors};root=Path(unreal.Paths.project_dir());fit=json.loads((root/'Art/Source/Aurelion/Z07CaptureKit/capture-fit.json').read_text());sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem);probes=[]
    for row in fit['components']:
        a=labels[row['actor']];assert a.get_actor_transform().export_text()==row['actor_transform']
        c=next(c for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.get_name()==row['component'])
        assert c.get_world_transform().export_text()==row['component_transform'] and str(c.get_collision_enabled())==row['collision']
        if row['instance_count'] is None:
            assert c.static_mesh.get_path_name()==row['mesh'] and a.get_actor_enable_collision()==row['actor_collision']
            o,e,_=unreal.SystemLibrary.get_component_bounds(c);ignored=[v for v in actors if v!=a]
            for dx in (-.4,0,.4):
                raw=unreal.SystemLibrary.line_trace_single(world,unreal.Vector(o.x+e.x*dx,o.y,o.z+e.z+50),unreal.Vector(o.x+e.x*dx,o.y,o.z-e.z-50),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,ignored,unreal.DrawDebugTrace.NONE,True)
                h=next((v for v in raw if isinstance(v,unreal.HitResult)),None) if isinstance(raw,tuple) else raw
                assert h and h.to_tuple()[0] and h.to_tuple()[9]==a and abs(h.to_tuple()[5].z-o.z-e.z)<.02
                probes.append(row['actor'])
        elif 'Post' in row['actor']:
            assert c.static_mesh.get_path_name()==row['mesh'] and c.get_instance_count()==1
            assert c.get_instance_transform(0,world_space=True).export_text()==row['all_instance_transforms'][0]
            assert not c.get_editor_property('visible') and c.get_editor_property('hidden_in_game')
        else:
            faction='Dominion' if 'DOMINION' in row['actor'] else 'Reformation';m=c.static_mesh;b=m.get_bounds();t=c.get_instance_transform(0,world_space=True);p=t.translation;s=t.scale3d;r=t.rotation.rotator();origin=a.get_actor_location()
            assert m.get_name()=='SM_Aurelion_KIT_Z07'+faction+'Capture' and c.get_instance_count()==1
            assert abs(p.x-origin.x)<.01 and abs(p.y-origin.y)<.01 and abs(p.z+900)<.01
            assert max(abs(v-1) for v in (s.x,s.y,s.z))<.001 and max(abs(v) for v in (r.pitch,r.yaw,r.roll))<.001
            assert abs(b.origin.z-b.box_extent.z)<.01 and abs(b.origin.z+b.box_extent.z-400)<.01
            assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game') and not c.get_editor_property('override_materials')
            assert sm.get_num_uv_channels(m,0)==2 and sm.get_nanite_settings(m).get_editor_property('enabled') and sm.get_convex_collision_count(m)==0 and sm.get_simple_collision_count(m)==0
    gates=[]
    for faction,beat in [('DOMINION','DestroyDominionResonator'),('REFORMATION','DestroyReformationCage')]:
        expected={n for n in labels if n.startswith('Z07_Cage_') and '_'+faction in n}
        candidates=[a for a in actors if a.get_class().get_name()=='SovAurelionJournalGate' and str(a.get_editor_property('beat_id'))==beat and a.get_editor_property('bound_visual_actors')]
        assert len(candidates)==1 and len(expected)==7
        g=candidates[0];targets={a.get_actor_label() for a in g.get_editor_property('bound_visual_actors')}
        baseline=next(row for row in fit['gates'] if row['beat']==beat)
        assert g.get_actor_label()==baseline['actor'] and targets==set(baseline['targets'])==expected
        assert str(g.get_editor_property('mission_id'))==baseline['mission']=='M12_FireAndFrost'
        assert not g.get_editor_property('block_after_completion') and not g.get_editor_property('use_gate_body')
        gates.append(dict(actor=g.get_actor_label(),beat=beat,targets=sorted(targets)))
    return dict(assemblies=2,retired_post_visuals=8,physical_contacts=len(probes),gates=gates,qualification='Saved editor geometry and exact journal references; destruction playback and live traversal remain unqualified.')
