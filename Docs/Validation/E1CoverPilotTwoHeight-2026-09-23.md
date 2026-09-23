# E1 cover validation pilot — two-height repair

The E1 input pilot selected a recovery point after one visibility ray at
140 cm, then checked different torso/head rays (50 and 150 cm above the pawn)
on arrival. The arrival check also treated a non-`None` Unreal trace tuple as
shelter even when its hit was non-blocking. A low coffer could therefore be
selected repeatedly and rejected on arrival, or misclassified as shelter.
These were validation-pilot defects, not changes to player or enemy gameplay.

`Scripts/Validation/Aurelion/continue_aurelion_e1_input.py` now interprets
Unreal's bare or tuple `HitResult` by its blocking flag and non-character
obstacle, and requires both torso and head rays against each current enemy at
selection and arrival. Candidate height follows the pawn's offset above its
navigation start point, so sloped endpoints use equivalent sample heights.
The focused Python suite (`test_e1_cover_navigation` and
`test_e1_combat_path_corners`) passed 9/9 checks, including non-blocking tuple
and head-exposure cases.

Two visible fresh M12 PIE runs used ordinary Enhanced Input after the
respective pilot changes:

| Run | E1/handoff | Native recoveries | Cover choices | Rejected on arrival | Player damage receipts |
| --- | --- | ---: | ---: | ---: | ---: |
| `E1CoverTupleRetest-20260923-161157-2894703e` | Passed | 3 | 21 | 19 | 114 |
| `E1TwoHeightCoverRetest-20260923-162140-a9c63a36` | Passed | 2 | 2 | 1 | 52 |

Both runs earned native E1 victory, completed the two physical holds, and
handed off to a ready Selene. Both passive enemy-pressure observers completed
without error, and the run reports confirm the project content was unchanged
through PIE. The second run finished in 158.1 seconds, versus 339.4 seconds in
the first. Because combat and enemy motion vary between runs, this pair
supports the corrected cover decision and exposes the previous churn; it does
not prove a fixed encounter completion rate or that normal difficulty is
pleasant. Two and three fatal recoveries remain too much to call E1 reliable
or fully tuned. Neither run played the later M12/M13 route.
