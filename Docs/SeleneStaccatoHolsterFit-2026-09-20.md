# Selene Staccato holster fit

The CP2 review showed Staccato projecting diagonally across Selene's shoulders
and behind her head. Its updated BackB fit carries the rifle almost upright along
her right back, with its stock lower and its barrel beside her head. Weapon scale,
BackA fallback, hand sockets and wield offsets remain unchanged.

## Evidence and scope

`SeleneLiveHolsters-20260920-121742-ee2d94e1` restored the authentic CP2 and read
the active visual ownership and both skeletal components. Staccato uses
`/Game/Fab/Sci-Fi_Sniper_Rifle_1/Animated/Staccato_Animated` on Socket_BackB.
Verity uses Socket_Verity in BackA; Axiom uses Socket_HipL. Staccato correctly
switches to weapon_r when drawn. Its size is not caused by an accidental scale.

The first preview failed before mutation because the editor disallows direct
assignment to the Offset member on a struct instance. Importing the complete
attachment struct is supported and preserves the other fields.
`SeleneHolsterFitEvaluated-20260920-122348-8a329271` tested a 35-degree adjustment;
it still crossed behind the head and was not saved.

`SeleneHolsterUpright-20260920-122604-2c958c20` tested the accepted 55-degree
upright adjustment, shifted 10 cm right and 15 cm down in the sampled standing
frame. These are authoring measurements; the resulting fixed offset follows
Socket_BackB during animation, rather than applying a world-space correction.
Standing, crouching, movement and drawn screenshots were inspected. The final
runtime relative transform is stored in `refine_selene_staccato_holster.py`.

The authoring script accepts only the observed original or reviewed new BackB
configuration, backs up the project asset before saving, checks all reflected
defaults by restoring its sole field edit, and preserves source weapon and M12
file hashes. It saves only WI_Staccato. The fresh-play checker verifies both
weapon skeletal components against their live expected socket and transform
through idle, crouch, movement, jump, landing, Verity draw/attack, Staccato draw
and restow. Gameplay input comes through existing APIs; no save bytes, grants,
character transforms or weapon size are changed for the test.

The preview used an embedded 844 x 550 game viewport. Movement frames reported
a small VRAM budget overrun while a second interactive editor was open; this is
not performance qualification. Full-screen, first-person, cover, companion and
all-angle clipping acceptance remain separate work.

## Saved result

`SeleneHolsterSaved-20260920-122808-1e664f44` saved only WI_Staccato and passed
the default-preservation and protected-file checks. The new asset SHA256 is
`7557B0A88CE99E2BBB15D4EF815D25371BF0AF3C8C188F4CC22D3F505A26EA76`.
Fresh reload `SeleneHolsterReloaded-20260920-122907-7bb6d7f8` passed all eight
cases and sixteen physical mesh/socket/transform checks. Verity's
AM_VerityTwin_01 was observed, Staccato moved to weapon_r when drawn, and both
components returned to the saved BackB fit on restow. Restow, jump and Verity
screenshots were inspected. The standing unarmed silhouette is clearer; the
Verity stance still changes projected head/barrel overlap, so this is not a claim
of zero overlap across every pose and camera angle.

Full baseline `20260920-121358-b25465b3` and final `20260920-122926-e5761b5b`
passed build (without SkipBuild), all 720 matching automation tests, report
coverage and source integrity. Python syntax and whitespace checks passed.
M12 retains SHA256
`B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5`.
Existing user ability, animation and plugin work remains untouched.
