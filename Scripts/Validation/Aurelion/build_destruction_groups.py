"""Validate the reviewed Z08 collision groups; never mutate campaign assets.

The explicit instance mapping is intentional: overlap alone must not admit a sky
sphere, trigger, or future mission barrier into a destructible state group.
"""
import argparse
import hashlib
import json
from pathlib import Path
import uuid

GROUPS = {
    'Z08_LC_Southwest': [0, 1], 'Z08_LC_WestSouth': [2, 3],
    'Z08_LC_SouthMid': [4, 5], 'Z08_LC_EastNorth': [6, 7],
    'Z08_LC_Northeast': [8, 9], 'Z08_LC_Northwest': [10, 11],
    'Z08_HC_WestOuter': [12, 13], 'Z08_HC_WestInner': [14, 15],
    'Z08_HC_NorthMid': [16], 'Z08_HC_EastSouth': [17, 18],
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def build(survey):
    require(survey['actor_count'] == 3140, 'Unexpected map actor count')
    rows = {r['source_instance_index']: r for r in survey['candidates']}
    require(len(rows) == len(survey['candidates']) == 19 and set(rows) == set(range(19)),
            'Cargo instances changed or are duplicated')
    groups = []
    for label, indices in GROUPS.items():
        members, obstruction = [], None
        for index in indices:
            row = rows[index]
            matches = [c for c in row['overlapping_colliders'] if c['actor'] == label]
            require(len(matches) == 1, f'{label}: missing or ambiguous collider for {index}')
            collider = matches[0]
            require(collider['class_name'] == 'StaticMeshActor' and collider['profile'] == 'BlockAll',
                    f'{label}: obstruction class/profile changed')
            require(all('ECR_BLOCK' in collider[key] for key in ('pawn_response', 'visibility_response')),
                    f'{label}: obstruction no longer blocks movement and shots')
            require(len(collider['center_probes']) == 2 and
                    all(p['contact_cm'] is not None for p in collider['center_probes']),
                    f'{label}: missing horizontal collision evidence')
            if obstruction is not None:
                require(all(obstruction[k] == collider[k] for k in
                            ('component', 'transform', 'bounds_cm', 'collision', 'profile')),
                        f'{label}: members disagree about the shared obstruction')
            obstruction = collider
            members.append({k: row[k] for k in ('source_instance_index', 'source_transform', 'bounds_cm')})
        actual_indices = sorted(index for index, row in rows.items()
                                if any(c['component'] == obstruction['component']
                                       for c in row['overlapping_colliders']))
        require(actual_indices == indices, f'{label}: obstruction spans additional cargo')
        union = [[min(m['bounds_cm'][0][axis] for m in members) for axis in range(3)],
                 [max(m['bounds_cm'][1][axis] for m in members) for axis in range(3)]]
        require(all(abs(union[side][axis] - obstruction['bounds_cm'][side][axis]) < .1
                    for side in range(2) for axis in range(3)),
                f'{label}: visual envelope no longer matches the obstruction')
        # Reject gaps/overlaps: these reviewed, axis-aligned boxes tile each group.
        volume = lambda b: (b[1][0]-b[0][0])*(b[1][1]-b[0][1])*(b[1][2]-b[0][2])
        require(abs(sum(volume(m['bounds_cm']) for m in members)-volume(union)) < volume(union)*1e-5,
                f'{label}: member volume does not cover the group envelope')
        for i, a in enumerate(members):
            for b in members[i+1:]:
                require(any(min(a['bounds_cm'][1][axis], b['bounds_cm'][1][axis]) -
                            max(a['bounds_cm'][0][axis], b['bounds_cm'][0][axis]) < .1
                            for axis in range(3)), f'{label}: overlapping members')
        identity = f'SovereignCallOrigins/M12/Z08/Destruction/{label}'
        groups.append(dict(placement_id=identity, reserved_save_guid=str(uuid.uuid5(uuid.NAMESPACE_URL, identity)),
                           collider=obstruction, members=members, bounds_cm=union,
                           break_policy='Break all member visuals and retire exactly this obstruction together',
                           campaign_enabled=False))
    return dict(schema_version=1, status='ownership_groups_validated_not_runtime_integrated',
                source_visual_component=survey['visual_component'], source_mesh=survey['mesh'],
                first_review_candidate='Z08_HC_NorthMid', groups=groups,
                excluded_overlap_components=sorted({c['component'] for r in rows.values()
                    for c in r['overlapping_colliders'] if c['actor'] not in GROUPS}),
                save_policy='Reserved GUIDs require explicit placed Narrative savable owners; do not derive identity from HISM indices.',
                reload_policy='Restore authored intact/broken state, visual visibility, collision and navigation together; do not replay fragment simulation.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('survey', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    raw = args.survey.read_bytes()
    result = build(json.loads(raw.decode('utf-8-sig')))
    result['survey_sha256'] = hashlib.sha256(raw).hexdigest()
    args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(f'Validated {len(result["groups"])} groups covering 19 visuals. Campaign remains unchanged.')
