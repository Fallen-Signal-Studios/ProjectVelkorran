"""Deterministic visual-only Z12 dock canopy placement from the authored kit manifest."""
import hashlib
import json


def plan(manifest):
    assembly = manifest['dock_assembly']
    assert assembly['berth_m'] == [28, 18]
    assert assembly['center_clear_lane_m'] >= 5
    rows = []

    def add(dock, role, index, asset, x, y, z):
        rows.append(dict(label='Aurelion_Z12_%s_Canopy_%s_%s' % (dock, role, index),
                         asset=asset, location=[round(x, 5), round(y, 5), round(z, 5)]))

    def height(x):
        return 7.5 + 1.2 * (1.0 - (x / 13.2) ** 2)

    for dock, cx in (('Dominion', -3800), ('Reformation', 3800)):
        for i, y in enumerate(assembly['rib_stations_y_m']):
            rib = ('SM_Aurelion_KIT_Z12CanopyTerminalRib' if i == 0
                   else 'SM_Aurelion_KIT_Z12CanopyVaultRib')
            add(dock, 'Rib', i, rib, cx, 47500 + y * 100, 0)
            for j, x in enumerate(assembly['foot_x_m']):
                add(dock, 'Foot', '%d_%d' % (i, j), 'SM_Aurelion_KIT_Z12CanopyBearingFoot',
                    cx + x * 100, 47500 + y * 100, 0)
        for i, x in enumerate(assembly['coffer_x_m']):
            for j, y in enumerate(assembly['coffer_y_m']):
                add(dock, 'Coffer', '%d_%d' % (i, j), 'SM_Aurelion_KIT_Z12CanopyCoffer_4x4',
                    cx + x * 100, 47500 + y * 100, (height(x) + .13) * 100)
        for i, x in enumerate(assembly['pendant_x_m']):
            for j, y in enumerate(assembly['pendant_y_m']):
                add(dock, 'Pendant', '%d_%d' % (i, j), 'SM_Aurelion_KIT_Z12CanopyPendant',
                    cx + x * 100, 47500 + y * 100, (height(x) - .1) * 100)
    assert len(rows) == 102 and len({r['label'] for r in rows}) == 102
    return rows


def digest(rows):
    return hashlib.sha256(json.dumps(rows, sort_keys=True, separators=(',', ':')).encode()).hexdigest()
