# Aurelion cold-start AI trace

This is an opt-in, read-only observation tool for the intermittent Selene/Hound startup stall recorded in [WorkPCAIStartupObservation-2026-09-06.md](WorkPCAIStartupObservation-2026-09-06.md). It does not repair the stall or establish its cause. It does not add goals, rescore activities, refresh/reset perception, select targets, change factions, grant abilities, or edit any asset.

## Morning procedure

1. Pull and build the Development Editor target normally. Restart the editor after the native rebuild; do not rely on Live Coding to install module startup delegates.
2. Open the real Selene encounter with the existing hostile Hounds. In the editor Output Log command field, **before pressing Play**, enter `sov.AIStartupTrace 1`. This changes a transient diagnostic switch only. It arms the next Game/PIE world at `OnPreWorldInitialization`. Enabling it after a stalled run captures nothing for that existing world; begin a fresh PIE run.
3. Start fresh PIE and play normally. Do not run probes that reset perception, alter health/Echo, regenerate goals or select a target. If the stall repeats, leave it untouched for approximately 60 game seconds. End PIE within the 120-second wall-clock observation window if possible. Heavy map startup and pauses count against that window.
4. Preserve the complete `Saved/Logs/ProjectVelkorran.log` as a separately named file before another editor launch. Repeat a cold start for a successful comparison. Keep runs, maps, current source commit, protagonist and observed outcome labeled independently. Instrumented runs have some timing overhead; compare with an uninstrumented cold run too.
5. Enter `sov.AIStartupTrace 0` when finished. The switch is unavailable in Shipping builds. A capture stopped by disabling the switch cannot resume for that world; start a new PIE world to obtain a fresh recording.
6. Extract the recorded timeline outside Unreal:

   ```powershell
   python Scripts/Extract-AIStartupTrace.py Saved/Logs/ProjectVelkorran.log --output Saved/Diagnostics/Selene-AIStartup.json
   ```

   The extractor preserves all trace events, groups worlds/captures, marks missing or truncated sequences, and reports each controller's attachment and first successful Sight callback. Its output explicitly does not assert an AI diagnosis.

## What the trace records

- World arming time, monotonically numbered events, capture identity, world path, network mode, elapsed wall time and game time. The observer is armed before the world initializes; the actual perception-listener attachment is separately recorded per controller.
- Entry to controller BeginPlay and goal-generator initialization, generator return, and the existing native perception callback. The native listener may run after Blueprint listeners; the trace does not claim to intercept the engine event before every consumer.
- PlayerState faction assignment and entry immediately before its player's `OnFactionUpdated` broadcast. An assignment without a matching publication can mean the PlayerState did not have a Narrative player pawn at that point. These are distinct from the GameState attitude-change broadcast, which is also recorded separately.
- Native ASC readiness-epoch publication and receiver replication entry. These do not collapse pawn/ASC existence, matching avatar ownership and a nonzero readiness epoch into a single assumed ready state.
- Read-only snapshots of controller/pawn/ASC identities and readiness, current factions, Narrative GameState availability, current behavior tree, activity state, goal counts by type, goal-generator identity/outer, current Sight actors, and player hostility/alive state.

The `alive` field is the existing Narrative gameplay predicate. That predicate returns true when a Narrative character has no ASC; it is not proof that Health or readiness initialized. Read it alongside the explicit ASC identity, avatar-match and readiness-epoch fields.

Snapshots are sampled at most every 0.25 seconds per observed controller, emit only on a state change or a five-second heartbeat, and are also taken at generator initialization and real native perception callbacks. A sampled transition identifies the first **observed** state, not the exact publication time. No extra actor tick is enabled. Controllers whose existing tick is disabled may have event snapshots but no periodic snapshots.

Capture is capped at 120 wall-clock seconds and 2,000 events per world. At most 32 controllers receive periodic snapshots. Lists include at most 32 Sight actors/generators/goal types and four player controllers; accompanying total counts expose list truncation. The current project is single-player, and this is intended for the small Hound encounter rather than a whole-campaign AI survey. World teardown clears diagnostic state; the module unregisters its world delegates on shutdown.

## How to evaluate a stalled run

Compare the order of `perception_attached`, `generator_initialize_enter/return`, `player_factions_assigned`, `player_faction_publication`, `asc_ready_publication`, and the first successful `perception_callback` with `sight: true`. Inspect that callback's target factions/ASC and the adjacent snapshot's GameState/generator/goal state. Then compare the same sequence in a successful cold start.

A successful Sight callback arriving while player factions are empty, followed by faction publication without another Sight callback and with zero attack goals, would support investigating the missing faction/readiness subscription. It does not by itself prove the Blueprint relay was delivered, the attack generator subscribed successfully, or that every other attack predicate passed. `generator_initialize_return` only establishes that the existing initialization call returned; it does not establish that its Blueprint GameState branch succeeded.

**Events before each native perception attachment remain unknown.** Current Sight actors are a snapshot, not a replay of prior callbacks. Missing callbacks in a truncated log or after a cap are also unknown. Absence of a GameState attitude broadcast does not imply absence of player faction publication. Do not derive a blanket perception reset or forced attack-goal repair from these logs.

## Verification status

The host extractor has eight failure-injection tests covering sequence gaps, missing arming, truncated captures, missing callbacks, malformed evidence and clocks, duplicate/reordered records, and distinct PIE captures. Run with:

```text
python -m unittest discover -s Scripts/Tests -p TestAIStartupTrace.py
```

Native changes require a UE 5.7 build and the cold-start procedure above. This environment has not compiled or executed the observer in Unreal. Existing successful builds and the prior 424-test report predate this observer and do not qualify it.
