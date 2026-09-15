"""Measured cache wall coverage; retain native collision and mission controls."""
from pathlib import Path
import json,runpy,unreal

def check_cache_enclosure(actors):
    root=Path(unreal.Paths.project_dir());source=root/'Art/Source/Aurelion/CacheEnclosureKit'
    baseline=json.loads((source/'native-baseline.json').read_text())['components'];labels={a.get_actor_label():a for a in actors}
    moves=json.loads((source/'skin-relocation.json').read_text())['rows']
    manifest=json.loads((root/'Art/Source/Aurelion/Z08WallKit/manifest.json').read_text());contacts=[]
    for move in moves:
        a=labels[move['actor']];c=a.static_mesh_component;old=next(r for r in baseline if r['actor']==move['actor'])
        assert a.get_path_name()==old['path'] and a.get_actor_transform().export_text()==old['actor_transform']
        assert c.static_mesh.get_path_name()==old['mesh'] and str(c.get_collision_enabled())==old['collision'] and str(c.get_collision_profile_name())==old['profile']
        o,e=a.get_actor_bounds(False);lo=[v-w for v,w in zip((o.x,o.y,o.z),(e.x,e.y,e.z))];hi=[v+w for v,w in zip((o.x,o.y,o.z),(e.x,e.y,e.z))]
        assert all(abs(v-w)<.001 for actual,expected in zip((lo,hi),move['bounds_cm']) for v,w in zip(actual,expected))
        skin=next(r for r in manifest['interior_coverage'] if r['original_index']==move['original_index']);assert skin['bounds_cm']==move['bounds_cm']
        axis=0 if e.x<e.y else 1
        for z in (-1150,-1060,-970):
            start=unreal.Vector(o.x,o.y,z);end=unreal.Vector(o.x,o.y,z);name=('x','y')[axis]
            setattr(start,name,getattr(o,name)-getattr(e,name)-10);setattr(end,name,getattr(o,name)+getattr(e,name)+10)
            hit=c.line_trace_component(start,end,False,False,False);assert hit is not None
            contacts.append(dict(actor=a.get_actor_label(),contact=hit[0].export_text()))
    presentation=labels['Aurelion_SupportBarrierView']
    authored_closures=presentation.west_barrier_visual.static_mesh.get_name()=='SM_Aurelion_KIT_WestCacheGate'
    if authored_closures:
        runpy.run_path(str(root/'Scripts/Editor/check_support_closures.py'))['check_support_closures'](actors)
    for old in baseline:
        if old['class_name'] not in ('SovAurelionPrioritySupport','SovAurelionSupportPresentation'):continue
        if authored_closures and old['class_name']=='SovAurelionSupportPresentation':continue
        a=labels[old['actor']];c=next(c for c in a.get_components_by_class(unreal.PrimitiveComponent) if c.get_path_name()==old['component'])
        assert c.get_world_transform().export_text()==old['transform'] and str(c.get_collision_enabled())==old['collision']
    assert len(actors)==3140
    return dict(relocated_skins=3,native_contacts=contacts,authored_closures_checked=authored_closures,qualification='Three wall skins match measured native solids. Authored closures are checked when present; live priority access and destruction remain separate work.')
