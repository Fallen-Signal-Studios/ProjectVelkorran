# Holographic HUD refinement

User direction: make the HUD more holographic and minimal, with Halo: Campaign
Evolved as a reference. Reference landing page:
https://www.halowaypoint.com/news/silent-cartographer-third-person-gameplay-halo-campaign-evolved

This pass replaces the solid survival frame with open faction marks and a
translucent optical backing. Tarrik keeps clipped amber corners; Selene keeps
fine cyan split corners. The line halo is static and faint. Resource bars are
slimmer, row gaps are tighter, and objective backing is lighter. Text shadows
retain contrast against the bright environment. Resource labels, values, native
bindings, ability readiness, safe-area ownership and quiet-HUD behavior remain
the same. High-contrast mode retains an opaque black backing and white signals.

Validation:

- UE5.7 Editor build succeeded (`HolographicHUD-20260913-Build.log`).
- Three existing CombatVitals tests passed (two success, one success with a
  no-game-viewport warning in the test fixture), no failures.
- Rendered in `HolographicHUDPIE-20260913-083805-dab2f3be`: Tarrik's open
  amber corners, thin bars and translucent backing were observed against the
  bright entry floor. Unmodified capture: `hud-bright-entry.png` in that run.
  A second capture, `hud-pressure-hall.png`, shows the dark floor and incoming
  fire warning. Fresh entry passed at 34.937 seconds; PIE was then stopped.
  These are bounded viewport samples, not all-background acceptance. Runtime
  styling is isolated in commit `a7b9738a`.
- The separate preceding run `DialogueWidthRetry-20260913-083057-167eadd7`
  passed fresh entry at 57.281 seconds and visibly expanded the subtitle/caption
  panels from short to long QA text. Character-based word splitting remains a
  separate issue. The first preview attempt used an unavailable Python widget
  library; the corrected lookup uses actual widgets owned by the current player.

The previously archived Win64 playtest predates this styling pass. No updated
package, full gameplay route, all-resolution acceptance or 90% alignment is
claimed by these changes.
