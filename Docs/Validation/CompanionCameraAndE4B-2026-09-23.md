# Companion camera collision and E4B replay — September 23, 2026

The inherited capsules of `BP_AurelionTarrikCompanion` and
`BP_AurelionSeleneCompanion` both blocked the Camera channel. Their Pawn
response remains Block and Visibility remains Ignore. The two Blueprint
defaults now ignore Camera, so a companion capsule cannot shorten the player
spring arm during close follow or combat. A fresh editor process reloaded both
saved assets and read Camera = Ignore from each class default object in
`CompanionCameraReloaded-20260923-135233-d6e21075`.

The visible, earned-checkpoint E4B replay
`E4BCameraAfterFix-20260923-135343-e03bf467` loaded the unchanged ArenaEntry
banks through the public save owner, used the authored retry, and passed the
ordinary frost/heat/Poise/Core and conventional combat route. All protected
people survived. Its passive observer recorded 248 live samples with Selene's
capsule Camera response = Ignore and no observer errors. Tarrik dealt 553.4
health damage in 45 receipts; Selene drew and attacked, dealing 111.2 health
damage in 13 receipts. This is one checkpoint replay after the content change,
not an uninterrupted M12–M13 route or proof of every companion scenario.

The terminal frame remains a visual issue. Tarrik was near a wall, the camera
was 122.2 cm from him, and the captured image is obscured by nearby level
geometry. Selene's actor bounds were 29.2 cm away from the camera at that
instant, and her capsule was ignoring Camera. This frame does not prove the
companion fix resolved all close-camera obstruction. The earlier
`E4BCameraMeshBounds-20260923-134349-41c937d7` frame placed Selene's bounds
over the camera with her capsule still blocking Camera; it motivated the
narrow Blueprint change. The two terminal frames are at different player
positions and are not a controlled visual A/B comparison. Camera/wall
readability remains open for the M12 polish pass.

The separate fresh-editor configuration audit
`AbilityAssetCurrent-20260923-132728-0fbfbfad` passed all ten player ability
assets: each has distinct alternating A/B cast montages and the required cast
Niagara and projectile FX references. This checks bindings, not execution of
every ability, weapon state, or impact presentation.
