"""Summarize completed passive route evidence without treating missing damage as a contact defect."""
import argparse
import json
import re
from pathlib import Path


def summarize(run):
    movement = json.loads((run / 'companion-observation.json').read_text(encoding='utf-8-sig'))
    animation = json.loads((run / 'companion-animation.json').read_text(encoding='utf-8-sig'))
    if movement.get('status') != 'stopped' or animation.get('status') != 'stopped':
        raise ValueError('Stop the passive observers before summarizing their final evidence')
    if movement.get('errors') or animation.get('errors'):
        raise ValueError('Observer errors prevent a clean evidence summary')
    result = {'scope': 'Observed route only; not full companion combat or visual acceptance',
              'run': str(run), 'heroes': {}, 'route': []}
    for hero in ('Selene', 'Tarrik'):
        frames = [f for f in animation['samples'] if f['identity'] == hero]
        attacks = [f for f in frames if any(
            a.get('montage') and ('_Attack_' in a['montage'] or '/AM_VerityTwin_' in a['montage'])
            for a in f['animations'])]
        protected = [f for f in attacks if 'TagName="Narrative.State.Invulnerable"' in (f.get('focus_tags') or '')]
        receipts = []
        for receipt in movement['damage']:
            raw = receipt['result']
            source = re.search(r'SourceActor="([^"]+)"', raw)
            if not source or 'BP_Aurelion' + hero + 'Companion' not in source[1]:
                continue
            health = re.search(r'AppliedHealthDamage=([0-9.]+)', raw)
            if not health:
                raise ValueError('Native receipt has no health-damage field')
            receipts.append(float(health[1]))
        distances = [f['focus_distance_cm'] for f in attacks if f.get('focus_distance_cm') is not None]
        result['heroes'][hero] = {
            'observed_frames': len(frames), 'attack_frames': len(attacks),
            'attack_frames_with_invulnerable_focus': len(protected),
            'attack_distance_cm': [min(distances), max(distances)] if distances else None,
            'damage_receipts': len(receipts), 'positive_health_receipts': sum(x > 0 for x in receipts),
            'applied_health_damage': round(sum(receipts), 6),
            'damaging_contact_observed': any(x > 0 for x in receipts),
            'all_observed_attack_focus_invulnerable': bool(attacks) and len(attacks) == len(protected),
            'interpretation': ('Damaging companion contact observed in this run.' if any(x > 0 for x in receipts)
                else 'No damaging contact established; inspect target protection and attack opportunity before changing assets.')}
    for path in sorted((run / 'E1Continuation').rglob('*input-continuation.json')):
        data = json.loads(path.read_text(encoding='utf-8-sig'))
        result['route'].append({'file': str(path.relative_to(run)), 'status': data.get('status'),
                                'reason': data.get('reason')})
    result['recorded_route_stages_passed'] = bool(result['route']) and all(r['status'] == 'passed' for r in result['route'])
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    args = parser.parse_args()
    run = args.run_directory.resolve(strict=True)
    output = run / 'companion-review-summary.json'
    if output.exists():
        raise FileExistsError('Preserve the existing summary: ' + str(output))
    output.write_text(json.dumps(summarize(run), indent=2), encoding='utf-8')
    print(output)
