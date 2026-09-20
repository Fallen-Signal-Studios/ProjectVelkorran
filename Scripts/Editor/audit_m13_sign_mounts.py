"""Read saved sign bounds and trace existing ceiling/floor supports. No saves."""
import json, os
from pathlib import Path
import unreal
out=Path(os.environ['SOV_AURELION_RUN_DIRECTORY'])
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Aurelion_M13'
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
labels=('Aurelion_Art_Sign_Z10_72eb71','Aurelion_Art_Sign_Z11_9bb415','Aurelion_Art_Sign_Z12_f79ad4')
rows=[]
def vector(p):return [p.x,p.y,p.z]
def trace(start,end):
    raw=unreal.SystemLibrary.line_trace_single(world,unreal.Vector(*start),unreal.Vector(*end),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,True,[],unreal.DrawDebugTrace.NONE,True)
    hit=next(v for v in raw if isinstance(v,unreal.HitResult)) if isinstance(raw,tuple) else raw
    if not hit or not hit.to_tuple()[0]:return None
    t=hit.to_tuple()
    return dict(actor=t[9].get_actor_label(),position=vector(t[5]),normal=vector(t[6]))
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if a.get_actor_label() not in labels:continue
    c=a.get_component_by_class(unreal.TextRenderComponent)
    o,e=a.get_actor_bounds(False);p=a.get_actor_location()
    rows.append(dict(actor=a.get_actor_label(),text=str(c.text),transform=a.get_actor_transform().export_text(),
        origin=vector(o),extent=vector(e),location=vector(p),size=c.world_size,
        horizontal=str(c.get_editor_property('horizontal_alignment')),vertical=str(c.get_editor_property('vertical_alignment')),
        material=c.get_material(0).get_path_name(),hidden=a.get_editor_property('hidden'),
        probes=[dict(x=x,ceiling=trace((x,p.y+10,p.z+100),(x,p.y+10,p.z+10000)),
                     floor=trace((x,p.y+10,p.z-100),(x,p.y+10,p.z-10000))) for x in (-200,0,200)]))
assert len(rows)==3
(out/'sign-mounts.json').write_text(json.dumps(dict(status='inspected',signs=rows),indent=2))
