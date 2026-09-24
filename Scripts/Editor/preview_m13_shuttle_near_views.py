"""Read-only player-height close approach captures for the two scenic berths."""
from pathlib import Path
import runpy
import unreal

root = Path(unreal.Paths.project_dir())
runpy.run_path(str(root / 'Scripts/Editor/preview_m13_route.py'), init_globals={
    'M13_ROUTE_VIEWS': [
        ('dominion-close', (-1500, 47500, 180)),
        ('reformation-close', (1500, 47500, 180)),
        ('dominion-dock', (-3800, 46400, 180)),
        ('reformation-dock', (3800, 46400, 180)),
    ],
    'M13_ROUTE_YAWS': {
        'dominion-close': 180, 'reformation-close': 0,
        'dominion-dock': 90, 'reformation-dock': 90,
    },
    'M13_ROUTE_PITCHES': {
        'dominion-close': 5, 'reformation-close': 5,
        'dominion-dock': 6, 'reformation-dock': 6,
    },
})
