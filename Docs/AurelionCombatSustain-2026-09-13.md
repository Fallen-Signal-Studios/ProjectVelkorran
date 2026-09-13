# Native combat sustain content

The refuge-validation route exhausted Cinderline in E3 after spending most of its
reserve in E1. A live census found no remaining ammo pickups. The subsequent
stopped-editor census identified a content defect: all seven enemy archetypes
had sustain enabled, but null ammo pickup class, ammo item class and Echo pickup
class. Native fatal-damage code requires those authored references before it
spawns anything.

`configure_aurelion_sustain_drops.py` binds the seven owned enemy Blueprints and
24 placed M12 enemies to the existing `/Game/Ammo/BP_AmmoPickup` and
`/Game/Ammo/BP_EchoPickup`. Their explicitly authored payloads are 63 rifle rounds
and 4 Echo. Those values now feed the native drop component instead of its
unconfigured 5/1 defaults. Both pickup assets retain their existing 20-second
lifetime, native overlap collection, reserve/Echo caps and presentation meshes.
Story NPCs are excluded. No pickups are placed in the map or spawned by a test.

The seven enemy Blueprints compiled and saved through the existing guarded
editor helper. The E3 ordinary-input driver now uses E1's bounded existing-pickup
path finder: matching ammo, unclaimed visible pickup, at most 18m away and a
complete native path. It changes movement input only. Empty ammunition without
a reachable pickup still stops the driver; melee fallback remains separate work.

Evidence lives in `Saved/Validation/Aurelion/SustainFallback-20260913`:
`defaults.json` records the null bindings and existing pickup payloads;
`configured.json` records all seven archetypes and 24 instances.

A fresh normal-order run was retained in
`Saved/Validation/Aurelion/SustainBindingsValidated-20260913-124036-262f254e`.
Its passive `sustain-observation.json` has observed native ammo/Echo actors owned
by defeated E1 drones, with the expected 63/4 payloads. At observer time 132.500s,
the native ammo and Echo pickups sourced from SecurityDrone_C_9 were both claimed.
Reserve rose from 58 to 121 and Echo from 29 to 33, while the clip remained 23.
The E1 input report records its ordinary pickup approach at driver time 140.000s.
This verifies one complete native kill/drop/approach/collection cycle. Route
completion and E4 survivor/Thermal acceptance are still pending. No gameplay resources,
damage, actor motion or campaign state are written by the observer.

E1 passed in 276.656s and E2 in 59.734s. E3 entry then stopped at the descent:
the complete native path projected the requested waypoint 76cm sideways, but
the pilot required arrival within 40cm of the unprojected point. It reached the
native endpoint and issued zero movement until its stall guard stopped the run.
The pilot now accepts a reached complete-path endpoint only within a bounded
100cm projection and the original floor-height tolerance. Six focused unit
cases pass, including partial-path, distant projection, wrong-floor and invalid
coordinate rejection. Fresh runtime validation is pending. This fixes test-driver
arrival bookkeeping, not level collision or companion movement.

This is the narrow combat-sustain exception recorded in `CampaignV2ChangeLog.md`,
not a persistent loot economy. It does not establish 90% alignment or qualify
packaged performance, audio, both priorities, hero cast likeness or the full
environment mesh pass.
