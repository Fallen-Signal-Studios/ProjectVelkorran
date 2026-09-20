# M13 independent-assent consoles

The two decorative stock console instances on `Aurelion_Art_M13_Z10_13_211d30` are replaced by an authored Aurelion lectern. It has fluted stone supports, gold mouldings, a recessed service coffer, a sloped three-panel control surface and twelve illuminated status registers. The material palette matches the chamber paving and wall panels.

Editable Blender source, FBX, studio image and manifest live in `Art/Source/Aurelion/Z10AssentConsole`. Rebuild with Blender 4.5 using `--background --factory-startup --python Art/Source/Aurelion/build_z10_assent_console.py`. The mesh has 10,252 source triangles, two UV channels, Nanite enabled and no generated collision. The narrow detail uses explicit position precision and a full fallback mesh.

Fresh survey: `M13ChamberFixtureAudit-20260920-023451-608ed98c`. The original vendor mesh had an offset pivot and nonuniform instance scale. The new asset uses unit scale at each original world-space bounds center: `(−250,34800,−1730)` and `(250,34800,−1730)` cm. Its geometry stays inside the former 100×70×120 cm envelope. FBX handedness is accounted for using a full reflection matrix so the inclined panels and front supports face the lower-Y approach.

Preview `M13AssentConsolePreview-20260920-023944-cebc333e` passed fitting and actor/collision preservation assertions. The studio image, chamber overview and close console capture were inspected. These are fixed editor game-view captures, not a new mission replay, interaction activation test or GPU performance qualification.

Only the decorative HISM mesh and its two placements change. Native assent actors, their gameplay properties, interaction components and collision remain untouched. The save wrapper backs up M13, saves only that map, reloads the new placements and checks the protected M12 hash. No journal semantics or content revision change is involved.

The full validation runner now offers the same optional `-UseFileSystemCache` recovery already present in the editor runner. It selects Unreal's installed filesystem fallback graph and a project-local cache, restores the caller's environment afterward, and leaves default behavior unchanged. This is needed because the normal Zen cache failed at startup during the preceding input work; no user cache was deleted or global configuration changed.

Saved/reloaded run `M13AssentConsoleSaved-20260920-024203-51d2bac7` passed all placement, actor/collision and protected-M12 checks. Its close console capture was inspected. M12 remains SHA256 `B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5`.

Full gate `20260920-024356-f4106c81` passed with build enabled, all 719 matching automation tests, coverage and source integrity, using the new optional filesystem-cache flag. The pre-change full baseline was `20260920-021610-06fce633`. No tracked edits occurred during the gate. The broader goal remains incomplete.
