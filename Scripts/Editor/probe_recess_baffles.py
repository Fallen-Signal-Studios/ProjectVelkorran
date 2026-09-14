"""Read-only sightline, capsule and native navigation checks for refuge baffles."""
import json
from pathlib import Path
import unreal

root=Path(__file__).resolve().parents[2]
out=root/'Saved/Validation/Aurelion/RecessBaffles-20260913'; out.mkdir(parents=True,exist_ok=True)
assert not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Aurelion_M12'
def hit(raw):
    if raw is None: return dict(blocked=False,actor=None)
    values=[v for v in raw if isinstance(v,unreal.HitResult)] if isinstance(raw,tuple) else [raw]
    assert len(values)==1 and isinstance(values[0],unreal.HitResult)
    p=values[0].to_tuple()
    return dict(blocked=bool(p[0]),actor=p[9].get_actor_label() if p[9] else None,impact=p[5].export_text())
marks={'Tharne':(-2750,22150,-1110),'Lyessa':(-2350,22150,-1110),'Malik':(3050,22150,-1110),
    'WestPatient1':(-2850,22400,-1110),'WestPatient2':(-2350,22680,-1110),
    'EastWalker1':(2890,22580,-1110),'EastWalker2':(3200,22680,-1110)}
lines=[]
for origin in ((-750,19263,-1020),(0,20800,-1020),(-1000,21600,-1020),(1200,21000,-1020)):
    for name,p in marks.items():
        target=unreal.Vector(p[0],p[1],p[2]+65)
        row=hit(unreal.SystemLibrary.line_trace_single(world,unreal.Vector(*origin),target,unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[],unreal.DrawDebugTrace.NONE,True))
        row.update(origin=list(origin),target=name); lines.append(row)
capsules=[]
for name,p in marks.items():
    row=hit(unreal.SystemLibrary.capsule_trace_single_by_profile(world,unreal.Vector(*p),unreal.Vector(p[0],p[1],p[2]+.01),42,88,'Pawn',False,[],unreal.DrawDebugTrace.NONE,True))
    row['mark']=name; capsules.append(row)
paths=[]
pending=unreal.SovAurelionNavigationLibrary.is_navigation_being_built_or_locked(world)
for name,start,end in (
    ('West refuge',(-2600,21750,-1110),marks['Tharne']),
    ('East refuge',(3050,21750,-1110),marks['Malik']),
    ('Medical cache approach',(-2600,21750,-1110),(-3050,22380,-1110))):
    nav=unreal.SovAurelionNavigationLibrary.find_path_to_location_synchronously(world,unreal.Vector(*start),unreal.Vector(*end),None,None)
    paths.append(dict(name=name,complete=bool(nav and nav.is_valid() and not nav.is_partial()),points=[p.export_text() for p in nav.path_points] if nav else []))
report=dict(stage=getattr(unreal,'_recess_probe_stage','before'),navigation_pending=pending,sightlines=lines,cast_capsules=capsules,paths=paths,
    scope='Static editor evidence; no runtime sight memory, pursuit, sustain interaction or survivor acceptance')
(out/(report['stage']+'.json')).write_text(json.dumps(report,indent=2),encoding='utf8')
unreal.log('RECESS_BAFFLE_PROBE '+report['stage'])
