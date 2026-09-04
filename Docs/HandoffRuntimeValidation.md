# Managed protagonist handoff runtime tests

Three native automation tests are provided in `SovHandoffRuntimeTests.cpp`, under `ProjectVelkorran.Campaign.Handoff`.

`StableActorIdentity` creates real project pawn/controller subclasses in a transient world. It verifies per-instance GUID uniqueness, stability across reads and that class defaults do not acquire runtime GUIDs.

`EarlyAdmissionAndSaveRejection` uses the production controller admission path to reject an unready handoff/invalid definition while retaining possession and idle transition state. It calls Narrative's explicit-slot save API with a missing PlayerState, which must return before any disk write. No successful disk-save path is executed by this test.

`ManagedReadinessAndSnapshot` uses an actual PlayerState ASC, attribute set, project Echo/Shield/Health/Poise components, Narrative actor/component serialization and `RestoreProtagonistSnapshot`. The fixture supplies the asset/visual readiness prerequisites that real asynchronous content would provide. The production readiness gate remains in control: missing visuals block it; matching saved currents restore before gameplay readiness; cross-hero data is rejected; final completion publishes readiness once; failure invalidates it.

These tests do not replace a map-based handoff validation. Real appearance streaming, default weapon grants, ability teardown, controller record loading, rollback to the origin, map travel, disk persistence and asset failure/timeouts still require the Unreal project with content. They have been authored and statically checked here but have not been compiled or executed because Unreal Engine is absent from this environment.

Run the campaign namespace through `Scripts/Validate-Unreal.ps1` in the UE5.7 workspace, then exercise same-map handoff and non-seamless M01→M02 travel with both a new destination protagonist and a previously saved destination snapshot. During callback tests, intentionally cancel or change ownership at each save/load/readiness notification and verify that the old transition epoch cannot mutate the replacement avatar.
