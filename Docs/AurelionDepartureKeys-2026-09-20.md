# M13 local departure key lights

Added two local movable RectLights to reveal the protagonists' armor at the separate departure positions. This supplements the earlier broad concourse fills. It is a visual refinement, not overall AAA/TDD sign-off or a repair for Selene's intermittent face color.

## Reviewed settings

`Scripts/Editor/aurelion_departure_keys.py` defines the shared preview/save specification:

- Tarrik: `(-1350, 48450, 260)`, 600 lumens.
- Selene: `(1350, 48450, 260)`, 240 lumens.
- Both: yaw -90, pitch -12; 950 cm radius; 220 by 180 cm source; neutral-warm `(1, .97, .92)` color; .65 specular scale; shadows enabled.

The local influence areas do not overlap. The lower settings preserve the amber/blue concourse contrast while revealing armor panels and edges. These two extra movable shadowed lights have not been GPU-profiled.

## Evidence

All folders below are under `Saved/Validation/Aurelion`:

- `DepartureKeyCompare-20260920-220154-05223d09`: six valid temporary PIE comparisons after earned public CP9 loads. The 2400/4800 lumen versions washed out both armor and skin and were rejected. Temporary lights/camera were removed; maps unchanged.
- `DepartureKeyBalanced-20260920-220509-03abbda1`: failed before lighting captures. The second-load checker saw a new world but the same controller pointer/hash and rejected it. No lighting result or gameplay fix is claimed. Whether this is address reuse requires a separate identity-lifecycle observation.
- `DepartureKeyLowOutput-20260920-220849-5133a5f0`: earned public CP9 load and six before/240/600 comparisons passed their observation checks; maps unchanged and temporary actors removed. Inspected all lower-output character views. Selected Tarrik 600 for stronger armor definition and Selene 240 to retain more restrained highlights. Selene's face was already unnaturally pale/gray in the zero-light baseline; the new light does not establish a skin fix.
- `DepartureKeySave-20260920-221326-f6af4ce2`: saved only M13, then reopened it. Both light descriptions exactly matched their selected preview values; all other actor transforms/collision remained identical. Includes the pre-change map backup.
- `DepartureKeySavedPlayback-20260920-221449-aa03fa54`: fresh earned public CP9 load passed the state/HUD checks and exact saved-light readback. No light was spawned or modified. Inspected `tarrik-600.png` and `selene-240.png`: Tarrik's copper armor remains readable and Selene's dark armor has clear panels/highlights. Her face appears naturally colored in this launch; that does not resolve the separate intermittent color problem. The temporary inspection camera was removed, the original view restored and both map hashes stayed unchanged.

The lighting observer now requests one earned CP9 load, which is sufficient for this comparison. The reusable checkpoint observer defaults to two loads as before and explicitly reports its requested load count; the existing repeat-load assertions were not weakened.

Saved M13 SHA256: `4b1e1f4458f8dc231bfb15325d3cdec5498bc04d39fd59ac684c0739cc884f3f`.
Protected M12 SHA256 remains `B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5`.

Pre-change full build/726-test gate: `20260920-215632-074058ad`. Final gate `20260920-221732-79c3404f` passed the full build, all 726 matching automation tests, report coverage and source integrity (99 warnings). Neither gate used SkipBuild. All preview/save/playback processes and the final gate are terminal.
