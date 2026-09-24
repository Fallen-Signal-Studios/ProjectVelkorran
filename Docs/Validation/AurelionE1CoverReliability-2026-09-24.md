# M12 E1 ordinary-input cover reliability, 24 September 2026

The Z11 art pass exposed an intermittent M12 E1 progression blocker: two fresh
CP0 routes stopped after four player deaths and three native retries before
they could enter M13. The M12 map, security-drone assets and input driver were
byte-identical to an earlier passing route. The failed runs sent only 22 and
59 attack-input frames; their first attempts did not damage a drone. This did
not establish an enemy-health, weapon-damage or map regression.

`E1DecisionProbe-20260924-080950-1ccb3af4` added read-only firing-decision
telemetry to the ordinary-input pilot and ran in visible, isolated Unreal PIE.
It sent 1,171 actual attack-input frames, damaged and killed drones, and still
ended after four deaths. The pilot made **zero** cover moves while the shield
was repeatedly empty. Its unchanged cover rule required a nearby navigable
point to occlude the player's centre and upper body from **every** live drone.
The enemy observer recorded 27 gunshots and 10 rockets during the first
24 seconds of E1; a prior passing run recorded 27 and 10 in the same interval.
This evidence does not justify increasing or decreasing authored enemy output.

`E1CoverSurvey-20260924-081546-48b736f0` recorded all cover-search results
without changing the pilot's decision. Its first low-shield search found nine
reachable points and a best point that blocked three of four drones, but no
fully sheltered point; the pilot rejected it. The next search blocked two of
three. A later fully sheltered point became available, and this route passed
the native six-drone victory, two route holds and Selene handoff with zero
deaths, 1,096 attack-input frames and two full-cover moves. Thus the encounter
can pass as authored, while a useful majority-shelter opportunity is sometimes
discarded. The repeated failures remain a reliability concern, not a proven
native combat defect.

The pilot now chooses a full-shelter destination first and, only if none is
reachable, may move to a point blocking at least 75% of live drones. It never
waits there for stationary shield recovery if any drone remains exposed; it
resumes movement and fire and delays the next search to avoid shelter churn.
This changes only the test pilot's normal Enhanced Input movement. No map,
weapon, enemy, health, ammunition, encounter or checkpoint state is written.
`test_e1_cover_navigation.py` covers the full/partial distinction, head-height
occlusion and the Unreal trace wrapper.

`E1MajorityCover-20260924-082200-40d3051b` started a new isolated,
visible M12 PIE from a fresh profile. The entry check passed. The new E1 pilot
used two majority-shelter moves and four full-shelter moves, sent 1,412 real
attack-input frames, defeated all six drones, completed both native holds and
handed off to a ready Selene. One death was recovered by the native retry
path. The enemy observer had zero errors; entry and E1 protected asset hashes
were unchanged. The M12 map still hashes to
`64a4517bb88719694793a46ae859f0eb6fbc861eef10e956e0574d8962b844a4`.
This proves the majority-shelter movement executes in live combat and does
not block the authored victory/handoff. A single pass cannot establish a
statistical improvement or qualify the later M12-to-M13 route, which remains
open alongside human play feel and final encounter tuning.
