# Interaction focus presentation

The continuous, heavy blue bounds rectangle seen around a corpse during E4B could cross
large portions of combat, captions and the Echo arc. The shared accessibility focus renderer
now draws short corner brackets on the same actual target bounds and a padded glass name
label. Tarrik/Selene use their existing protagonist accent palettes by default. Custom team
colors and color-vision presets remain supported; high contrast uses white strokes and a
black label plate. The outline toggle and thickness setting remain authoritative.

Each corner is suppressed where its full stroke footprint intersects occupied HUD, wheel,
objective, caption or subtitle geometry. Labels retain the existing safe-area placement.
Focus presentation also requires a ready, living player and clears during cinematics and
owned HUD-hide states. This does not change interaction range, target selection, holds,
corpse loot, or gameplay collision. The existing all-corners-on-screen projection rule is
unchanged; partially off-screen targets can still have no bounds indicator.

Full native gate `20260920-161136-24af511d` passed the build, all 722 tests, coverage and
source-integrity checks. This is a presentation change; no duplicate geometry-mirroring
unit test was added. Rendered acceptance must come from actual engine captures.

## Review qualifications

- `InteractionBrackets-20260920-161409-bb02b240` captured five settings around a real
  corpse, but the target extended off-screen. Visual inspection found no bracket strokes;
  it is not visual acceptance despite the observer completing successfully.
- `InteractionBracketsVisible-20260920-161801-848a4a9a` required all projected corners.
  Its gameplay child earned genuine E4B victory, but the observer found no suitable corpse
  focus before completion. See the thermal retry document for the independent victory proof.
- `InteractionBracketsVictory-20260920-162310-1bfd02d2` successfully restored that earned
  ArenaExit autosave. Defeated NPCs were omitted from restoration, so no corpse was available.
- `InteractionBracketsFramed-20260920-162830-702ff21c` ended before visual capture on a
  bounded HeatConfirm hold-focus failure. The focus alternated between the heat and frost
  request actors; this is an observed interruption, not a verified native root cause.
- `InteractionBracketsCamera-20260920-163138-319d96a7` ended on player death before a
  qualifying corpse target was available. It does not qualify presentation or victory.

`review_interaction_brackets.py` now reaches the authored MovePartner console through
ordinary inputs and uses normal interaction focus, then pauses for controlled settings
captures. The shared renderer is the same, but console review does not qualify close-up
corpse framing, Selene's theme in this specific indicator, or post-death visual retirement.
Those remain explicit visual follow-ups. The broader Aurelion goal remains active.

## Console visual acceptance

`InteractionConsoleGlass-20260920-163446-f4957d7e` completed all five settings captures.
The target was the real Aurelion.E4.MovePartner interactable, acquired by normal input and
interaction detection before the review camera framed it. All eight projected bounds
corners were recorded inside the 1696 x 862 viewport (DPI scale 0.79774445). The gameplay
child was intentionally stopped for presentation; it is not a new encounter pass.

All five images were inspected. Normal shows four short amber corners and a dark glass
label. The 200% label remains clear of the active subtitle. High contrast shows white
corners/text and an opaque black plate. The custom-color case switches both corners and
label to magenta. Disabling interaction outlines removes both. No continuous bounds
rectangle remains. The editor exited normally and M12's protected SHA256 remained
B7CEEAB512272FC80FE1E3B3454B08DF40BF40BC0E065C3C90993B910D6780D5.

This is acceptance of the shared renderer on the console, with the corpse, Selene and
post-death visual qualifications above still open. No claim of a fully finished M12/M13
visual pass or overall 90% alignment follows from these captures.
