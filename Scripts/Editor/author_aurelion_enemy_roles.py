"""Author only the Aurelion enemy roles, and write a receipt of what changed.

`setup_aurelion_enemy_roles` deliberately does nothing on import: it exposes
`build_enemy_assets(output_dir)` and the project's only existing caller is the full route
author, which also writes maps and encounters. This wrapper calls that one entry point so the
pass stays inside /Game/Aurelion/Enemies, which is the scope that module promises.

Run with a stopped editor and no PIE. It writes no gameplay state and edits no world.
"""
import json
import os
from pathlib import Path
import sys
import unreal


def run(output_directory=None):
    out = Path(output_directory or os.environ['SOV_AURELION_RUN_DIRECTORY'])
    out.mkdir(parents=True, exist_ok=True)
    report = dict(status='failed', scope='Aurelion enemy role definitions, ability and activity configurations')
    try:
        # The authoring module lives beside this one; the editor runs scripts by path.
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        from setup_aurelion_enemy_roles import build_enemy_assets
        report['result'] = build_enemy_assets(str(out))
        report['status'] = 'passed'
    except Exception as exc:
        report['error'] = str(exc)
        raise
    finally:
        (out / 'aurelion-enemy-authoring.json').write_text(json.dumps(report, indent=2, default=str), encoding='utf8')
        unreal.log('AURELION_ENEMY_AUTHORING ' + str(report['status']) + ' ' + str(out / 'aurelion-enemy-authoring.json'))
    return report


if __name__ == '__main__':
    run()
