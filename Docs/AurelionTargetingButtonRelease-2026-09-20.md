# Targeting key-layer verification

This pass tests the actual saved gamepad mappings through Unreal's simulated key path. It does not change gameplay content.

The fixture holds LB and presses Y, X and D-pad Left/Right separately, observes native semantic events, releases LB, then checks Y returns to Ability3. It checks for forbidden base-action events during the LB hold. Unreal 5.7's `Input.+key`/`Input.-key` implementation calls `PlayerController::InputKey`, so the saved mappings and blockers execute. This still bypasses physical hardware and Slate/CommonUI preprocessing, and semantic delivery alone does not prove target acquisition or companion obedience.

Initial run `TargetingKeyLayer-20260920-022028-74785616` passed weak assertions but recorded an extra press at each release boundary. Stricter confirmation `TargetingKeyReleaseConfirm-20260920-022313-427c8cbe` failed: all four targeting actions produced `[press, release, press, release]`.

An explicit button-down trigger was tested as a possible content correction. `TargetingButtonDownVerify-20260920-022852-56c1a13f` reproduced the same failure. That attempted change was rejected: `IMC_Combat` was restored from its exact pre-experiment backup and has no git diff. The original authoring script was also restored. The rejected experimental script and asset backup remain under the authoring run in Saved for diagnosis; they are not production changes.

The subsequent fixture neutralizes a forced key to zero before removing forcing during cleanup. This isolates an immediate-removal boundary that can interact with queued forced input. Its result must be inspected before classifying the extra events as a gameplay defect. A passing result would qualify this simulated-key fixture, not physical gamepad release behavior.

No weapon is wielded in this fixture, so unchanged empty wield state is explicitly not weapon-preservation evidence. Real wheel selection, menu routing and target acquisition remain open. No mission map is saved.

Fresh run `TargetingNeutralRelease-20260920-023111-d5065bc4` passed on the restored, original input asset. Each targeting event sequence was exactly `[press, release]`; all four chord actions arrived, no forbidden base semantic presses were observed while LB was held, and Ability3 arrived after LB release. This isolates the earlier duplicate to the immediate forced-input removal procedure in this fixture, rather than establishing a production mapping defect. Unreal exited normally.

No C++ or production content change remains. The latest full build/719-test gate is `20260920-021610-06fce633`; the new evidence here is the fresh simulated-key integration run. Physical gamepad/CommonUI behavior remains unqualified.
