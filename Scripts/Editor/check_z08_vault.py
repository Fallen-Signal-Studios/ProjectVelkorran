"""Saved vault geometry and isolated native doorway checks."""
import json
from pathlib import Path
import unreal

def check_z08_vault(world,actors):
    labels={a.get_actor_label():a for a in actors};root=Path(unreal.Paths.project_dir())
    source=root/'Art/Source/Aurelion/Z08VaultKit';fit=json.loads((source/'vault-fit.json').read_text())
    a=labels['ART_Crucible_UpperVault'];c=a.static_mesh_component;m=c.static_mesh
    assert a.get_actor_transform().export_text()==fit['vault_transform']
    assert m.get_name()=='SM_Aurelion_KIT_Z08VaultAssembly' and not c.get_editor_property('override_materials')
    assert not a.get_actor_enable_collision() and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
    assert c.get_editor_property('visible') and not c.get_editor_property('hidden_in_game')
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    assert sm.get_num_uv_channels(m,0)==2 and sm.get_nanite_settings(m).get_editor_property('enabled')
    assert sm.get_convex_collision_count(m)==0 and sm.get_simple_collision_count(m)==0
    origin,extent=a.get_actor_bounds(False)
    assert abs(origin.z-extent.z+500)<.1 and abs(origin.z+extent.z-1040)<.1
    assert abs(extent.x-3570)<.1 and abs(extent.y-2470)<.1
    assert [c.get_material(i).get_path_name() for i in range(c.get_num_materials())]==fit['materials']
    for row in fit['bands']+fit['physical_walls']:
        a=labels[row['actor']];c=a.static_mesh_component
        assert a.get_actor_transform().export_text()==row['transform'] and c.static_mesh.get_path_name()==row['mesh']
        retired=row in fit['bands']
        assert a.get_actor_enable_collision()==(False if retired else row['actor_collision'])
        if retired:assert c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
        else:assert str(c.get_collision_enabled())==row['collision']
        if retired:assert not c.get_editor_property('visible') and c.get_editor_property('hidden_in_game')
    retained=[labels[r['actor']] for r in fit['bands']+fit['physical_walls']];ignored=[a for a in actors if a not in retained];probes=[]
    for y in (18400,23200):
        for x in (-1000,-200,0,200,1000):
            raw=unreal.SystemLibrary.capsule_trace_single(world,unreal.Vector(x,y-350,-1105),unreal.Vector(x,y+350,-1105),42,88,unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,ignored,unreal.DrawDebugTrace.NONE,True)
            hit=next((v for v in raw if isinstance(v,unreal.HitResult)),None) if isinstance(raw,tuple) else raw
            blocked=bool(hit and hit.to_tuple()[0]);assert blocked==(abs(x)==1000),(x,y,blocked)
            if blocked:assert hit.to_tuple()[9] in [labels[r['actor']] for r in fit['physical_walls']]
            probes.append(dict(x=x,y=y,blocked=blocked))
    manifest=json.loads((source/'manifest.json').read_text());assert len(manifest['coffer_placements_m'])==42
    assert len(actors)==3140
    return dict(coffers=42,actor_count=len(actors),retired_bands=2,doorway_capsules=probes,qualification='Stopped-editor geometry and native doorway collision; live wall-running, cinematics and GPU performance remain unqualified.')
