# E1 input pilot — capsule, partial paths and return fire

This follow-up to [the two-height cover check](E1CoverPilotTwoHeight-2026-09-23.md)
keeps changes in the ordinary-input validation pilot. It does not change enemy
health, damage, rate of fire, encounter rules or level geometry.

A read-only editor census (`E1CoverHeightVerified-20260923-163558-b564d974`)
measured Tarrik's capsule at 88 cm half-height and 34 cm radius. The prior
head sample at +150 cm from his actor centre was outside the capsule. The pilot
now checks centre and +75 cm upper body at both cover selection and arrival.
The census also confirmed low coffer tops at about 112 cm, one stack at 229 cm
and tall buttresses at 544 cm; these are physical silhouettes, not claims that
any one spot protects against moving drones or rocket splash.

`E1CapsuleCoverRetest-20260923-163859-0430bd06` was stopped as a diagnostic,
without a terminal gameplay result. Its pilot stayed still behind an occluded
target because it discarded a valid partial Recast path with a 271 cm reachable
prefix. The pilot now follows only a partial path's reachable corners when the
prefix advances at least 100 cm, then recalculates. Focused corner tests cover
both a useful partial path and a path with no meaningful advance.

The next visible PIE run,
`E1PartialPathRetest-20260923-164739-4d4675b4`, followed partial paths and
switched away from occluded targets four times, but failed after its fourth
native player death at 193.2 seconds. It had 87 incoming native damage
receipts, 171 gunshots (72 damaging) and 74 rockets in the passive observer.
The pilot stopped firing whenever it selected cover, including during up to
ten seconds of exposed travel. That is a validation-input flaw: ordinary
player controls allow moving and firing together.

The pilot now returns fire while travelling to shelter when it has a clear
shot, and stops aiming/firing only while actually waiting in cover, reloading
or evading. A fresh visible PIE replay,
`E1CoverReturnFire-20260923-165430-ca60da80`, passed E1 victory after one
native recovery, completed both physical holds and handed off to ready Selene
in 153.2 seconds. It recorded three cover choices, two later exposure
rejections, two occluded-target switches and 41 incoming damage receipts. The
passive enemy observer completed with zero errors: 102 gunshots, 37 damaging
shots and 40 rockets. The report confirmed that project assets were unchanged
through play. Eleven focused Python checks passed.

This pair shows that the revised pilot can finish while still receiving real
enemy pressure. It does not establish a reliable completion rate, player feel
with physical keyboard/mouse, or that E1's difficulty is settled. The failed
run is preserved rather than erased by the later pass.
