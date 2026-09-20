# Earned quarantine checkpoint retry

The earlier `CompanionAfterSeleneIntegration-20260920-083002-e270cf0c` probe
loaded its genuine generation-18 E4A checkpoint successfully but failed its
assumption that the encounter should automatically become Active. That report
remains failed. It is not evidence of a broken save or companion attack.

`ASovEncounterDirector::Load_Implementation` deliberately parks arena-entry
records in Failed and holds their participants until explicit retry.
`ASovCampaignEncounterObjective::OwnsInitialEntryRetry` requires Inactive;
its overlap retry cannot release a loaded Failed encounter. The authored
`SovAurelionRequestActor` retry interaction is the intended route.

Isolated `EarnedRetryInput-20260920-095542-b1a37535` copied the same unmodified
checkpoint banks into its own profile and received native load SUCCESS. The
actual retry at (220,18750,-1153) was admitted and focused, displaying
"Retry encounter". Three movement-input frames, ten look frames and two
interact frames restored the encounter to Active, with a valid attempt,
Selene at 100 health, five enemy participants and seven protected people alive.
The journal did not change and the Aurelion content hashes remained unchanged.
The probe ended PIE and exited normally. No encounter-start API, teleport,
resource grant or campaign-journal mutation was used.

`aurelion_retry_input.py` encapsulates that ordinary navigation/look/interact
path. The earned companion bootstrap now uses it after an E4A checkpoint load,
releases input before handing control to the combat driver, and records its
input frames and focus/admission samples. It bounds interaction waiting and
refuses to qualify a retry that activated without its interact input.

The isolated result proves explicit retry can restore this earned E4A entry.
It does not prove companion damage, complete encounter victory, physical
controller input, rendered prompt quality, or arbitrary checkpoint coverage.

Integrated replay `CompanionEarnedRetry-20260920-095956-3eb603cf` also completed
the explicit retry, cleared the objective error and entered the existing E4A
combat driver. Its retry adapter recorded four movement, nine look and two
interact frames. The later driver failed its 150-second earn-Echo deadline:
it issued no attack input and recorded no precision shots. The final weapon-ray
sample hit the intended head zone but had only two consecutive samples where
its gate requires three. This identifies a driver qualification limit; it does
not establish a weapon or damage failure. The process exited normally and
Aurelion assets were unchanged.

The passive companion observer completed 380 samples in its separate 100-second
bound without an attack montage or a native source-damage receipt. Distances
ranged from 69.50 to 3007.02 cm. This remains insufficient to qualify Tarrik's
combat, and is not rewritten as a pass merely because retry succeeded.

`run_tarrik_focused_contact.py` isolates the remaining question using the same
earned retry and a public FocusTarget request without a player attack. It does
not advance the campaign combat driver. Contact
samples now also include game time, velocity, companion command/disabled state,
and both actors' gameplay tags to distinguish protection or dispatch conditions
from failed collision contact.

The first focused run, `TarrikFocusedContact-20260920-100611-8cf10b99`, retained
the observer's Staccato trigger. At 8.609 seconds the player's native receipt
applied 53.200001 health damage and killed the selected Linkbound. This removed
the intended live attack opportunity before a companion damage receipt. Its
100-second observation ended without errors but does not qualify Tarrik.
The final focused wrapper therefore supplies no player combat input; enemy
health, weapon damage and protection are unchanged.

`TarrikCommandContact-20260920-101047-0e30a71b` reached the bootstrap's
210-second startup/load bound before starting contact observation. Its final
status is timeout; the wrapper subsequently stopped PIE and exited normally.
The command-only path remains unqualified. The successful earlier retry runs
do not turn this timeout into a pass, and neither focused run proves Tarrik's
ordinary follow-combat behavior. A future command-only check must first secure
a ready restored session and then observe the actual command and target state.

Python compilation and whitespace checks passed. Final full validation
`20260920-101610-87372b25` ran without SkipBuild and passed the editor build,
all 720 automation tests, report coverage and source integrity. No tracked
edits occurred while that gate ran. The preceding unchanged native baseline was
`20260920-094902-8342bd39`, also a full build and 720-test pass.
M12 retains SHA256
`B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5`.
No gameplay C++, assets, mission definitions or generated localization were
changed; creator grenade edits and the local GASPALS plugin remain untouched.
