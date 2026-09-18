---
name: audit-item
description: Work an adversarial-audit item in Project Velkorran end to end — verify the finding against the real code, implement, write an automation test, prove the test fails without the fix, full build, commit. Use this whenever the user names an audit ID (PC2-08, AR2-17, EA2-06, UX2-15, CN2-04 and the like), says "next audit item", "keep going" or "proceed" while working through the audit, asks to fix something from Docs/AdversarialAudit-*, or asks for TDD-alignment work on the systems side — and also when they simply describe a defect that turns out to have an audit entry, even if they never mention the audit at all.
---

# Working an audit item

The audit in `Docs/AdversarialAudit-2026-09-17/` is a list of findings, not a list of instructions.
Each entry was written by a reviewer reading code, sometimes without building or running it. Treat an
entry as a **lead worth checking**, and let the code decide what is actually true.

## 1. Read the finding in full

```bash
grep -rn "<ID>" Docs/AdversarialAudit-2026-09-17/*.md
```

Read the whole entry, not the summary line. Entries cite file and line numbers; those drift. Find the
code by name rather than trusting the line number.

## 2. Verify the claim before writing anything

This is the step that most changes the outcome, and it goes both ways:

- **The finding may be worse than described.** AR2-17 was reported as a mis-selection between banks.
  Reading the code showed a downgraded build consumed *both* banks of a slot within two saves, leaving
  the player's progress only in a support file they would never find. The fix that matched the audit's
  description would have left most of the defect in place.
- **The finding may not be real.** Some entries rest on an assumption nothing in the repo can confirm.
  If verifying the premise is impossible, say so and decline the item rather than implementing against
  a guess. Declining with a reason is a good outcome; the item can come back when the thing that would
  verify it exists.
- **Adjacent code often has the same hole.** After changing how a newer-schema bank is classified, the
  cloud-export path would have started reporting such a slot as empty — the same data loss by another
  route. When you change a classification or a flag, grep every consumer of it before you stop.

Never conclude from scanning a `.uasset`'s packed bytes. Load the asset. Two confident and wrong
claims have come from string-grepping binaries.

## 3. Decide and state the scope

Say what you are fixing and what you are deliberately leaving. If the honest fix involves a trade-off
— giving up one property to protect a more important one — choose it, implement it, and name it in the
report as a choice rather than letting the user discover it. Offer the inverse if they would prefer it.

If the remaining work is content or Blueprint authoring, it is not yours. Write it into a handoff
document in `Docs/` with exact asset paths and property names, verified by loading or grepping.

## 4. Implement

Put the decidable part in a `Sov*Policy` namespace (`SovCameraPolicy`, `SovResonancePolicy`,
`SovCompanionApproachPolicy`) so the rule is testable without a world. Keep the component or subsystem
responsible only for gathering inputs and applying the result.

Comments explain the defect the code exists to prevent, and cite the audit ID. The reader a year from
now needs to know why the branch is there, not what it does.

## 5. Write the test, then prove it can fail

Write the test. Then **disable the fix, rebuild, and confirm the test fails.** That bar holds for
every P1 and P2. For a P3 whose fix is small and obvious, implementing and testing is enough - skip
the disable-and-prove step unless the fix is subtle enough that you cannot tell by reading whether
the test would catch its absence. The point is proof where a silent regression would cost something,
not ceremony on a one-line change.

Where the bar applies, it applies because a test written after a fix usually passes for the wrong
reason, and you cannot tell which by reading it.

Keep a small toggle script in the scratchpad that comments the fix out and back in, so the round trip
is two commands rather than hand edits you might not fully revert.

When the failures come back, read them one by one and ask of each: *would this assertion have failed
for the right reason?* Some will have passed either way:

> Staging the newer save in bank B looked fine, but bank B is the one the rotation avoids anyway, so
> three assertions about preserving it passed with the fix disabled. Moving the fixture to bank A —
> the bank the code actually targets — made all six discriminate.

An assertion that passes in both configurations is testing nothing. Move the fixture into the path the
code really takes, or drop the assertion.

Watch for the reverse too: a test can pass for the wrong reason because production already supplies
what the fixture adds. A blackout test once passed because the pawn already owned the component the
fixture was adding a second copy of.

Conventions, and the test-world gotchas that each cost an hour, are in `CLAUDE.md`.

## 6. Build fully, then run everything

```powershell
.\Scripts\Validate-Unreal.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.7' -DisableAura
```

No `-SkipBuild` for the run that precedes a commit. Skip-build does not recompile untouched files, so
it cannot see an include-order break or anything else that only a clean compile catches — one sat on
the branch green for an hour because every run used it.

Note the test count and that it went up by the number you added.

## 7. Commit

One item per commit. Subject is a declarative sentence about what changed for the player or the code
— *"Stop a downgrade from eating the save it cannot read"* — and the body explains the defect, names
the audit ID, and says why this is the fix. End with:

```
Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
```

Stage explicit paths. Never `git add -A`: the creator's `.uasset` and `.umap` edits live in the working
tree and are not yours. Never push.

## 8. Report

Say plainly:

- What the defect actually was, if it differed from the audit.
- **What you verified and what you did not.** "I disabled the fix and confirmed the test fails" is a
  claim worth making; "the schema tripwire is covered by an existing test, I did not re-prove it" is
  worth making too. Do not let the second kind go unsaid.
- Any trade-off you chose, framed as a choice with its inverse available.
- What is left, and what is now unblocked.

If you were wrong about something earlier in the session, correct it in a sentence and move on.
