# Build the combat playtest

Close Unreal Editor before a build. With UE 5.7, the project's plugins, Visual Studio C++ tools, and the Windows SDK installed, run from the repository:

```powershell
.\Scripts\Cook-CombatPlaytest.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.7' -ExcludeMetaHumanAuthoringData
```

The script builds the Development Editor and Game targets, then cooks, stages, and archives `/Game/Maps/Development/L_TarrikCombat` and `/Game/Maps/Development/L_SeleneCombat` to `Saved/WorkPCPlaytest`. Supply `-ArchiveDirectory` for another destination. Use `-SkipBuild` only when both native targets already contain the current source. UAT and per-target logs are retained under `Saved/Validation/PlaytestCook-*`.

The opt-in `-ExcludeMetaHumanAuthoringData` profile adds the cooker option `-NeverCookDir=/MetaHumanCharacter/BuildPipeline+/MetaHumanCoreTech/RealtimeMono`. The existing project descriptor restricts the owning authoring/capture plugins to Editor targets. Cooking their default pipeline and smoothing assets for the game target therefore reports unavailable classes. This profile excludes only those two authoring-data directories. Runtime character meshes, materials, textures, grooms, RigLogic, and normal hard/soft dependency traversal remain included. It changes no project packaging settings or plugin target allowlists and suppresses no cook errors. Reassess the profile if those plugins become runtime dependencies in a future project revision.

The script also explicitly includes `/HairStrands/Emitters/StableRodsSystem` through the cooker's `-PACKAGE` option. UE 5.7's GroomComponent dynamically loads both built-in hair solvers when simulation starts, while the groom dependency list normally includes only its selected solver. This includes the missing engine asset without editing either character or its physics settings.

The standard unmodified cook and the scoped retry logs are retained separately. After archive creation, test both maps with the actual packaged executable. The September work-PC validation also uses isolated per-map user data for its headless startup smoke checks; rendered combat and respawn are verified separately in Unreal Editor.
