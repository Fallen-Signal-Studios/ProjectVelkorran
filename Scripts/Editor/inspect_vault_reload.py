"""Diagnose serialization differences without modifying the editor world."""
import json
import re
from pathlib import Path
import unreal

out=Path(__file__).resolve().parents[2]/'Saved/Validation/Aurelion/CrucibleVault-20260913'
baseline=json.loads((out/'physical-baseline.json').read_text())
actors={a.get_name():a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()}
rows=[]
for name,b in baseline.items():
    for c in actors[name].get_components_by_class(unreal.InstancedStaticMeshComponent):
        old=b['components'][c.get_name()]['instances']
        current=[c.get_instance_transform(i,world_space=True).export_text() for i in range(c.get_instance_count())]
        if old==current: continue
        differences=[]
        for i,(a,z) in enumerate(zip(old,current)):
            if a==z: continue
            av=[float(v[1:]) for v in re.findall(r'=-?\d+\.\d+',a)]
            zv=[float(v[1:]) for v in re.findall(r'=-?\d+\.\d+',z)]
            differences.append(dict(index=i,max_numeric_delta=max(abs(x-y) for x,y in zip(av,zv)),before=a,after=z))
        rows.append(dict(actor=name,label=b['label'],component=c.get_name(),old_count=len(old),new_count=len(current),changed=len(differences),maximum=max(d['max_numeric_delta'] for d in differences) if differences else None,samples=differences[:2]))
(out/'reload-instance-differences.json').write_text(json.dumps(rows,indent=2),encoding='utf8')
unreal.log('VAULT_RELOAD_DIFFERENCES_RECORDED')
field=actors['BP_AsteroidField_Globular_C_1']
properties={}
for key in dir(field):
    if not any(word in key.lower() for word in ('seed','random','generat')): continue
    try:
        value=field.get_editor_property(key)
        properties[key]=str(value)
    except Exception:
        pass
(out/'asteroid-properties.json').write_text(json.dumps(properties,indent=2),encoding='utf8')
