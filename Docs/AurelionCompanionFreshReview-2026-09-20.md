# Companion review after Selene posture integration

At `7e820577`, a fresh ordinary-input M12–M13 run passed all nine recorded
stage reports through SeparateDepartures. Both protagonist companions appeared,
followed, held their intended weapons and played attack montages. This turn
changed no gameplay, animation, damage or map assets: the new evidence does not
justify changing weapon reach or damage to compensate for protected targets.

Evidence: `Saved/Validation/Aurelion/CompanionFreshReview-20260920-083229-53141c0f`.
Its `companion-review-summary.json` is reproduced by
`Scripts/Validation/Aurelion/summarize_companion_review.py <run_directory>`
after stopping the observers. The summarizer refuses live/incomplete observer
files, observer errors and an existing output. It distinguishes observed damage
from attack frames aimed at invulnerable actors, and never assigns overall
companion-combat acceptance from a route pass.

| Observation | Selene | Tarrik |
|---|---:|---:|
| Recorded animation frames | 1,687 | 908 |
| Weapon-attack frames | 48 | 6 |
| Attack frames with invulnerable focus | 0 | 6 |
| Native source damage receipts | 9 | 0 |
| Applied health damage | 101.81818 | 0 |

Selene damaged the Elite and WallRunner. Her Verity attack animations played
with the corrected mesh frame and native light-attack grant. This adds fresh
companion combat evidence after integrating the GASPALS posture layer.

Tarrik's six sampled sword-attack frames occurred at 68.13–104.08 cm from the
same Linkbound, whose health remained 6.2434769. Every frame's focus tags include
`Narrative.State.Invulnerable`. The phase director deliberately suspends and
protects surviving participants at the handoff boundary; that mechanism is
consistent with these observations, but the samples do not identify the exact
owner of the target's tag. Zero damage here does not establish failed sweep
contact. Tarrik still needs an ordinary attack opportunity against a living,
unprotected target before his damage behavior can be qualified.

The preceding isolated checkpoint run
`CompanionAfterSeleneIntegration-20260920-083002-e270cf0c` loaded the genuine
generation-18 E4A checkpoint successfully, but its encounter entered Failed
before the contact observer could start. Its report is retained as failed;
the subsequent fresh route does not retroactively validate that checkpoint.

The fresh route passed E1, E2, meeting, survivor rescue, quarantine entry,
both quarantine phases, native M13 travel and M13 through separate departures.
Quarantine completion retained all protected people alive. The route used
ordinary Enhanced Input and native interactions, without supplying damage,
health, ammunition, actor transforms or journal receipts. Live Unreal views
were inspected in the embedded editor viewport during opening combat,
quarantine and the M13 ending. This does not establish full visual fidelity,
physical controller coverage, packaged execution or checkpoint reload safety.

PIE was explicitly stopped and both companion observers finalized without
errors. Unreal was left open. M12 retained SHA256
`B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5`;
the creator's map, grenade assets and local GASPALS plugin remain untouched.
The prior full build/719-test gate was `20260920-080421-63f9ba55`.
The final full build, all 719 automation tests, coverage and source-integrity
checks passed in `20260920-085054-f8f139eb`. The isolated editor was closed
briefly for module relinking and reopened after validation.
