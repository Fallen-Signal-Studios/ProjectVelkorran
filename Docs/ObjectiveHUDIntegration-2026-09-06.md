# Native objective HUD integration

This slice connects the existing campaign objective lifecycle to the existing local accessibility presentation. The August 14, 2026 TDD sections 9.11–9.12, 13.2–13.4, and 17.2 are the design authority. Campaign state remains the only objective owner.

## Behavior

- The native frontend consumes `GetActionableObjectiveIds` from its own controller's campaign component. It admits only the current mission, a matching possessed protagonist, `Available` or `Active` states, authored objective labels, and knowledge that this protagonist actually holds.
- Active objectives lead the list. Main objectives precede optional objectives within the same lifecycle state, with authored order preserved within each priority. Canon-critical goals use the player-facing label **Main objective**. Optional goals are explicitly labeled **Optional**.
- A maximum of three candidate rows retains the original localized `FText` and any authored optional failure rule. Internal beats with empty labels and undiscovered/future beats are excluded, including from the remaining count.
- **Accessibility > Review current objectives** opens the existing accessible record menu with all authorized goals, including HUD overflow and complete failure rules. Its scrollable text and existing optional narration use the same transient cache as the HUD. Cache changes and clears immediately update an open review and cancel stale narration.
- Beat, objective, evidence, mission, and restore delegates refresh the view immediately. Transition changes, pending loads, missing/mismatched pawns, invalid state, unavailable storage ownership, and deferred account selection clear it. Interrupted ownership can resume only when the original namespace and local user are authoritatively available again; an actual owner change remains fenced until a valid campaign restore. Authorized offline desktop profiles remain supported without an online sign-in.
- EndPlay removes all campaign, transition, and account delegates. A presentation generation also rebuilds the view after removal/reconstruction without waiting for another campaign mutation.
- `bShowObjectiveText` lets the player hide the panel. Text uses independent `UIScale`, white text with explicit labels, and the existing high-contrast preference. The panel does not accept input or change campaign state.

## Layout boundary

The upper-left panel is inside the existing `USafeZone`. Text wraps within 38% of the safe width, bounded by the scaled compact panel width. Native row measurement budgets the panel below active sound captions and above speech subtitles. A row is shown only when its complete text and failure rule fit. Lower-priority rows are deferred whole and included in the remaining count. When no safe vertical space remains, the objective panel disappears until space returns; critical speech and captions retain priority. The existing subtitle and caption layout/reading clocks are unchanged.

This avoids painting partial failure rules over other critical information. Deferred text remains recoverable through **Review current objectives**, and the overflow message names that destination. This does not qualify authored text for every locale or create the full Current Mission menu with completed beats and evidence. The production content pass must keep immediate goals concise and exercise long localized labels, maximum UI/subtitle scales, 80–100% safe areas, split screen, ultrawide views, and simultaneous speech/captions.

## Regression coverage and evidence limit

Eight new Editor-only native tests cover:

1. Real campaign events presenting immediate goals, retiring success/failure, showing optional labels and failure rules, and prioritizing active goals.
2. Serialized restore rebuilding the view and invalid restore clearing it.
3. Protagonist mismatch, unpossession, widget reconstruction, teardown, and late delegate safety.
4. Independent UI scaling, hidden-objective preference, three-row admission, and a remaining count that excludes future beats.
5. Native choice resolution removing both the selected result and superseded alternative, then showing the reconciled route.
6. Native save-account admission, access loss, same-account recovery without reload, and an actual account replacement staying fenced until validated restore; subsequent objective events cannot reopen an unauthorized view.
7. Native widget measurement retaining a fitting single goal without reserving unused overflow text, and deferring whole rows when enlarged critical text exhausts the safe height budget.
8. The real accessible record menu displaying every authorized goal and full failure rule, excluding future text, and immediately clearing stale content without a navigation event.

These tests are registered source coverage. Unreal compilation, Slate execution, and rendered visual checks have not run in this workspace. Qualification must run the `ProjectVelkorran.UI.Objectives` tests in UE 5.7 and then the packaged M12–M13 route. The companion canonical state correction ensures an already-active protagonist-neutral goal also becomes unavailable to a protagonist missing its required knowledge; the HUD retains an explicit defense check.

## Content needed for the production route

The M12–M13 mission assets must supply localized `ObjectiveText` and readable `FailureRuleText` for goals that may fail. This slice neither creates content assets nor invents story labels from internal beat IDs. Authoring and packaging the handoff, local choice interaction, navigation markers, full mission details, and their console presentation remain separate gates.
