# Camera content audit

Read-only exports from `FloorLifecycleRoute-20260919-151656-ebd57345/CameraContent` cover BP_SovTarrik, BP_SovSelene, both Narrative protagonist parent Blueprints, and BPI_GameplayCamera. The two parents derive from the native SovTarrikCharacter and SovSeleneCharacter classes. `export_camera_content_readonly.py` follows the parent references in Unreal's text export, handling both UTF-8 and UTF-16 exports. It does not compile or save assets. Initial attempts to read an unexposed Python parent property and assume UTF-8 failed before mission startup; the corrected export completed with no findings in the same editor session.

None of these four protagonist Blueprint graph exports references ApplyCameraState, OnCameraStateChanged or GetCameraState. The protagonists still contain SetupCamera, Cycle Camera Mode, SetCameraMode and Get_CharacterPropertiesForCamera. The latter supplies the camera interface's struct: its CameraStyle pin comes from the existing select path, while CameraMode has no incoming link. Stance, IsAiming, aiming FOV and normal FOV retain their existing inputs.

The interface exposes Get_CharacterPropertiesForCamera and Get_MountPropertiesForCamera; it is not a camera mode/style setter. The handoff's instruction to route the resolved state to the interface therefore needs to be implemented through the supplied character-property struct. The existing SetCameraMode function accepts NewCameraStyle and also changes loose gameplay tags and plays a UI sound; blindly calling it on every resolved state change would import those side effects. Its old style decision path and input-cycle behavior must be reconciled with the arbiter rather than adding a competing writer.

The native component provides both GetCameraState and OnCameraStateChanged. Resolve invokes ApplyCameraState and broadcasts that delegate only when the state changes. These are available content integration points without changing C++. No camera implementation or runtime framing acceptance is claimed by this audit. The next authoring pass must preserve the existing FOV/stance/aim behavior and verify raise weapon, lower to threat focus, then drop focus to each protagonist's resting profile.

## Live getter failure found before authoring

`observe_camera_route.py` was attached read-only to the same retained replay during E2/E3 and continued across M13 travel. Selene's native state requests Balanced/FreeCam while the Blueprint getter returns Far/FreeCam. During Tarrik's actual rescue combat, the native state requests Close/Strafe at Aim priority but the getter still returns Far/FreeCam, false IsAiming and zero FOV fields. The enum exports confirm NewEnumerator0 is Far for style and FreeCam for mode; Balanced is style NewEnumerator1 and Close is NewEnumerator2.

A more immediate fault precedes the arbiter wiring: Get_CharacterPropertiesForCamera enters K2Node_DynamicCast_1 targeting `/NarrativePro/Pro/Core/BP/Framework/BP_NarrativePlayerController`. Its CastFailed pin has no outgoing execution link, so the function returns default struct values when that cast fails. `camera-controller-cast.json` verifies the actual BP_AurelionPlayerController is not a child of that legacy Blueprint, is a child of BP_SovPlayerController, and has FOVMultiplier 1.0. Repair this stale controller reference before judging the remaining camera-state integration. `repair_protagonist_camera_controller_casts.py` is prepared but has not yet been executed; no camera assets were saved during this mission replay.

## Controller reference repair after the completed replay

After the full M12–M13 chain passed and PIE ended, `repair_protagonist_camera_controller_casts.py` ran in the retained editor. The existing scoped native authoring API remapped 31 hard references across the explicit writable set (the two protagonists and the replacement controller required by the API). BP_SovPlayerController, BP_SovTarrik and BP_SovSelene each compiled with zero errors and zero warnings. Only BP_SovTarrik and BP_SovSelene were saved. The vendor controller's persistent fingerprint was unchanged. All three original disk assets were backed up under the run's `CameraCastRepair` directory.

Post-repair protagonist exports contain the owned controller reference and no legacy Narrative controller path. Both project-owned protagonist assets are now explicitly allowlisted for version control. No C++, map, vendor package or controller asset was saved. The creator's M12 map hash remains unchanged. Fresh runtime verification and the post-change full gate are pending; this repair alone does not implement the camera arbiter or qualify framing.

Fresh `CameraCastRoute-20260919-153756-8c86f34e` passed real M12 entry and E1 through the actual Selene handoff (E1 approximately 126.88 seconds). The camera trace now returns FOV 90 for both protagonists instead of zero. In ordinary Tarrik combat it reports IsAiming true and AimingFOV 67.5, then IsAiming false after lowering the weapon. Live Unreal review confirms the closer aiming view and Selene's cyan HUD after handoff. This verifies the getter/controller repair, not completed arbiter integration: the authored resting getter still returns Close/Strafe for both characters, while the native profiles request Far/FreeCam for Tarrik and Balanced/FreeCam for Selene. That remaining ownership/wiring mismatch is explicit follow-up work.

The camera observer finalized on PIE end without findings, and the editor was closed. Full validation `20260919-154256-b1ec2a17` passed the build invocation without SkipBuild, all 719 automation tests (95 warnings), coverage and source integrity. No tracked files changed during the gate. Pre-change baseline: `20260919-151130-03d3c571`. BP_SovPlayerController's disk hash still matches its pre-repair backup, confirming it was not saved on editor exit. The creator-owned M12 and grenade changes remain excluded.

## Partial Tarrik arbiter presentation wiring

`CameraArbiterAuthor-20260919-154756-f338df2a` backed up both protagonists, then authored Tarrik's `Get_CharacterPropertiesForCamera` in the Blueprint editor. GetCameraControlComponent feeds GetCameraState; enum selects translate its resolved Style and Mode into the authored camera-interface enums. Their outputs now feed the existing MakeStruct CameraStyle and CameraMode pins. Far/Balanced/Close/FirstPerson and FreeCam/Strafe map explicitly; Unchanged falls back to Far/FreeCam. Stance, aiming state, both FOV inputs and OverrideRig retain their previous wiring. Compilation succeeded. The read-only `CameraContent/BP_SovTarrik.t3d` export records these connections without findings.

Fresh ordinary-input M12 entry and E1 combat passed through the real Selene handoff in this session. E1 took approximately 109.22 seconds. The camera observer finalized without findings: all 24 Tarrik samples matched the native style and mode, with 13 Far/FreeCam profile samples and 11 Close/Strafe aim samples. Lowering the weapon repeatedly returned to Far/FreeCam. Normal FOV remained 90 and weapon aiming FOV reached 67.5. Live review showed the rendered over-shoulder combat view and the cyan HUD after Selene's handoff. The untouched Selene getter still supplied Close/Strafe while its native profile requested Balanced/FreeCam.

After ending PIE, only BP_SovTarrik was saved, and the editor exited cleanly. The creator-owned M12 hash remains `B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5`. This is uncommitted partial integration, not completed camera acceptance. Selene, legacy SetCameraMode/cycle/load behavior and tag side effects, event/delegate presentation handling, threat-focus fallback, shoulder use, and checkpoint reload remain open. In particular the old setter still writes CameraStyle and tags while the new getter consumes resolved native state; manual cycling is not yet qualified. No threat-focus claim occurred in this replay. The required post-change full gate is pending.

Post-change full validation `20260919-162108-45945a0b` passed the build invocation without SkipBuild, all 719 matching automation tests, report coverage and source integrity. No tracked files were edited during the gate. This validates the partial increment; it does not close the remaining camera migration or visual acceptance work.

## Selene mapping and fresh E1/E2 replay

`CameraSeleneAuthor-20260919-162300-6378c1fc` loaded the saved Tarrik change and backed up both assets before editing. In the Blueprint editor, the four native-state reader/translation nodes were copied into Selene's camera getter and connected to its existing MakeStruct style and mode pins. Selene's Unchanged-style fallback is Balanced. Existing stance, aiming flag, weapon aiming FOV, normal FOV and OverrideRig behavior remains intact. Compilation succeeded. The run's read-only `CameraContent` exports verify both output links, the native component/state links and the enum option values. Tarrik's three mapping nodes were repositioned for selection; its runtime wiring did not change.

The fresh ordinary-input replay passed M12 entry (62.92 seconds), E1 through the actual Selene handoff (136.80 seconds), and E2 combat plus both native receiver receipts (141.95 seconds). The camera observer finalized cleanly after PIE ended with no findings. All 60 samples matched native style/mode: Tarrik had 21 Far/FreeCam and 19 Close/Strafe samples; Selene had 11 Balanced/FreeCam and 9 Close/Strafe samples. Selene's normal FOV stayed 90 and Staccato aiming FOV was 36. Live viewport review showed both palettes, Selene's wider resting frame, her close aiming frame and live upper-right ammunition. These are runtime observations, not fullscreen reference-fidelity acceptance or a claim about untested threat-focus/cycle/load behavior.

The E2 replay exposed a separate interaction robustness issue despite eventually passing. At the east receiver, Selene was approximately 190.82 cm away and native CanInteract admitted the receiver, with aiming error under .001 degrees, but the current focus was `BP_AurelionEnforcer_C_4.NPCInteractable`. The corpse lay beside the terminal in the viewport. The aim-interaction stage lasted roughly 58 seconds before focus admitted the actual receiver and the native hold completed. The corpse was absent in the later successful view; expiry is a plausible cause, not established by a dedicated lifecycle trace. Do not call this prompt behavior qualified. Investigate the authored interaction priorities/reach and corpse lifecycle without altering the creator-owned M12 map or fabricating a receiver receipt. The native selector uses priority, facing, distance and admission, so an admitted corpse can compete with an admitted mission control.

After stopping PIE, scoped saves wrote only BP_SovSelene and BP_SovTarrik. The creator-owned M12 hash is unchanged. Both camera changes remain uncommitted WIP until legacy cycle/load/tag ownership and the remaining handoff acceptance are resolved. Baseline gate: `20260919-162108-45945a0b`. Post-change full validation is pending.

Post-change full validation `20260919-164033-415ec066` passed the build invocation without SkipBuild, all 719 matching automation tests, report coverage and source integrity. No tracked files changed during the gate. No C++ or map edits are part of this increment. This closes the gate for the saved partial mapping, not the remaining camera or broader mission requirements.

## Tarrik manual preference migration, partial

`CameraLegacyAudit-20260919-164252-c5486793` backed up both current protagonist assets. Tarrik's SetCameraMode now stores the preferred authored style, releases its previous ManualCameraClaim, requests the translated native style at Profile priority with reason ManualCameraPreference, and retains the returned handle. Mode and shoulder remain Unchanged so stronger requests can supply them. The default preference is now Far, matching Tarrik's built-in profile. This was authored in the Blueprint editor and compiled successfully. Only Tarrik was saved in this increment; the map and creator grenade assets were not saved.

Correction to the initial audit: PlaySound2D is present in the old setter but its execution input is disconnected. It was not playing on each setter call. The NarrativeSavableActor.Load callback, rather than BeginPlay, calls SetCameraMode with the saved CameraStyle. That save/load caller and Cycle Camera Mode now reach Tarrik's native request through the setter, but actual input cycling and checkpoint restoration have not yet been exercised.

Fresh ordinary-input M12 entry passed in 56.53 seconds. A separate **controlled Blueprint API test**, `manual-camera-controlled.json`, exercised Far, Balanced, Close, FirstPerson, repeated Far, a manual Balanced change under a synthetic Aim-priority claim, and release back to Balanced. All seven checks passed. Each getter matched the resolved style/mode, and each sample had exactly one ManualCameraPreference claim. The prior Far preference was restored after the test. This is a direct API boundary test, not proof of physical input, threat-focus gameplay or checkpoint acceptance. The entry session was stopped after this check; no combat completion is claimed.

Remaining work is explicit: Selene's manual setter is still legacy; first-/third-person tags still follow preferred CameraStyle instead of receiving resolved-state changes. Extract and bind idempotent perspective-tag presentation to OnCameraStateChanged, keeping manual preference separate. Also verify the retained handle is transient/non-SaveGame, actual cycling/load, real threat-focus fallback, and shoulder presentation. The camera assets remain uncommitted WIP. Baseline full gate was `20260919-164033-415ec066`; post-change full gate pending.

Post-change full gate `20260919-170600-42d58054` passed the build invocation without SkipBuild, all 719 matching automation tests, report coverage and source integrity. No tracked edits occurred during validation. The exported ManualCameraClaim property flags are 65541 (no SaveGame flag); explicit transient treatment remains to be authored. This gate validates the partial increment, not complete camera or reference-HUD acceptance.

## Selene manual preference and callback checks

`CameraPreferenceSelene-20260919-170802-21320467` reloaded Tarrik's saved Far default successfully and backed up both current protagonist assets. Seven request/translation/handle nodes were copied through the Blueprint editor into Selene's SetCameraMode. A Selene-owned ManualCameraClaim variable was created to resolve the copied getter/setter, then compilation succeeded. The existing preference setter now executes ReleaseCamera, RequestCamera, stores the returned handle and continues the existing perspective-tag branch. Selene's default preference is Balanced. Tarrik's nodes were repositioned only; its execution wiring did not change. Both assets were saved after PIE ended; the creator-owned M12 hash remains unchanged.

Fresh ordinary-input entry passed in 71.67 seconds and E1 passed in 99.20 seconds through real combat, the route hold and the actual Selene handoff. The read-only camera observer ended cleanly with no findings and 18 samples, all matching native style/mode: nine Far/FreeCam, seven Close/Strafe and two Balanced/FreeCam. The final Balanced sample includes the restored preference following the controlled test. Live review showed working Tarrik ammunition/radar in combat and the cyan Selene HUD after handoff.

The separate `selene-manual-camera-controlled-v2.json` passed 16 direct Blueprint API checks: seven style/override/fallback cases, eight calls to Cycle Camera Mode with Up=true, and the NarrativeSavableActor.Load callback. Every sample had exactly one ManualCameraPreference claim and matching interface style/mode. The upward cycle clamps at FirstPerson rather than wrapping; the observed sequence was Close, FirstPerson, then FirstPerson for the remaining upward calls. Balanced was restored afterward. The first test report preserves a harness error: the initial cycle call omitted its required Up? argument, after all seven style checks had passed. The corrected call passed. These are API checks, not physical control or real serialized checkpoint qualification; no E2 completion is claimed.

Remaining camera work: move perspective tags to idempotent resolved-state presentation, bind the component state-change delegate, explicitly mark runtime handles transient, verify downward/manual physical controls and real checkpoint restoration, test actual threat-focus fallback, and author the shoulder parameter. Both protagonist setters now request native ownership, but tag presentation is still incomplete and the assets remain uncommitted WIP. Baseline full gate: `20260919-170600-42d58054`; post-change full gate pending.

Post-change full validation `20260919-172155-45cd4693` passed the build invocation without SkipBuild, all 719 matching automation tests, report coverage and source integrity. No tracked edits occurred during the gate. This validates the saved increment and does not close the remaining camera, HUD fidelity or broader gameplay requirements.

## Resolved perspective presentation

`CameraPerspectiveAuthor-20260919-200501-5b157475` backed up both current
protagonists before Blueprint authoring. Both now implement
`ApplyResolvedCameraPerspective`: it reads the native resolved Style and adds
only the matching first-/third-person tag, using the existing no-duplicate add
option and clearing the opposite tag. SetCameraMode retains preference/claim
ownership and calls this presentation function rather than its old tag branch.
Possessed-client SetupCamera binds OnCameraStateChanged to the function and
applies the initial state. No other delegate listeners are cleared. Both
ManualCameraClaim variables are explicitly transient (exported flags 73733)
and not SaveGame. Both assets compiled successfully and were saved individually;
no map or source was saved.

The first controlled runtime check passed all ten Tarrik cases: four styles and
three repeated first-person -> synthetic Aim Close/Strafe -> first-person cycles.
The checks assert interface style/mode, mutually exclusive perspective tags and
one manual claim, then restore the previous preference. They do not qualify
physical input or checkpoint serialization. The normal route then stopped at
weapon-wheel selection: raw mouse direction was processed, but the widget's
LastNonZeroDelta stayed zero while the Blueprint tab remained foreground. The
original failed entry result is preserved. Fresh level-viewport replay is
`CameraPerspectiveReplay-20260919-204937-08687aad`; Selene and route proof and the
post-change full gate remain pending. Protected M12 hash is unchanged.

Fresh saved-asset replay `CameraPerspectiveReplay-20260919-204937-08687aad`
passed all 20 controlled perspective checks (ten per protagonist). The full M12
entry, E1, E2, meeting, rescue, E4 entry, E4A and E4B route passed, followed by
native M13 travel. M13 reached independent assents, GrammarPropagation and CP7,
then failed at the lift: the accepted interaction returned to AtOrigin before
movement. This does not qualify a completed M13 route. Physical camera controls,
serialized checkpoint restoration, real threat-focus fallback and the shoulder
rig remain open. Post-change full gate is recorded separately below.

Post-change full gate `20260919-211249-bd0f8c59` passed the build invocation
without SkipBuild, all 719 matching automation tests, report coverage and source
integrity. No tracked edits occurred during validation. This qualifies the saved
camera and subtitle increment, not the outstanding M13 or visual-quality work.
