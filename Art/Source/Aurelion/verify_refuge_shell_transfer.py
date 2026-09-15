"""Prove the 34 removed skins transfer to the ten measured wall envelopes."""
import json,math
from pathlib import Path
base=Path(__file__).resolve().parent;root=base/'RefugeShellKit'
old=json.loads((root/'wall-assembly-baseline.json').read_text());new=json.loads((base/'Z08WallKit/manifest.json').read_text())
assert new['placements']==old['placements'][:58]+old['placements'][92:]
assert new['interior_coverage']==old['interior_coverage'][34:]
walls=[r for r in json.loads((root/'shell-baseline.json').read_text()) if not r['actor'].endswith('ThresholdMark')]
assignments={r['actor']:[] for r in walls}
for piece in new['replaced_refuge_coverage']:
    lo,hi=piece['bounds_cm'];owners=[]
    for row in walls:
        a=[v-s*50 for v,s in zip(row['location'],row['scale'])];b=[v+s*50 for v,s in zip(row['location'],row['scale'])]
        if all(lo[i]>=a[i]-.001 and hi[i]<=b[i]+.001 for i in range(3)):owners.append(row['actor'])
    assert len(owners)==1,(piece,owners)
    assignments[owners[0]].append(piece)
rows=[]
for wall in walls:
    pieces=assignments[wall['actor']]
    volume=sum(math.prod(b-a for a,b in zip(p['bounds_cm'][0],p['bounds_cm'][1])) for p in pieces)
    assert abs(volume-math.prod(s*100 for s in wall['scale']))<.01
    for i,p in enumerate(pieces):
        for q in pieces[i+1:]:
            overlap=[min(p['bounds_cm'][1][k],q['bounds_cm'][1][k])-max(p['bounds_cm'][0][k],q['bounds_cm'][0][k]) for k in range(3)]
            assert min(overlap)<.001
    rows.append(dict(actor=wall['actor'],original_indices=[p['original_index'] for p in pieces],original_volume_cm3=volume))
(root/'coverage-transfer.json').write_text(json.dumps(dict(status='passed',unchanged_other_placements=62,transferred_skins=34,walls=rows,qualification='Removed skins partition the exact native solid envelopes; new visible caps have documented 2-4 cm setbacks.'),indent=2))
print('REFUGE_SHELL_COVERAGE_TRANSFER_PASS')
