# Rendered blood comparison

The eight existing owned blood systems were rendered in an unsaved Unreal studio, using their current materials and native-sized effects. No blood asset, gameplay code or mission map was changed.

`Scripts/Editor/review_blood_rendered.py` places Hit, Slash, Burst and Low variants in red and black rows. It explicitly advances Niagara simulation and pauses before each screenshot. Requested ages are 0.12, 0.30 and 0.60 seconds; fixed 60 Hz advancement gives 7, 18 and 36 ticks (the first is actually 0.1167 seconds). Each age reinitializes the effect; random seeds are not locked, so these are independent samples rather than a continuous animation or matched particle-by-particle color comparison.

Accepted visual evidence: `Saved/Validation/Aurelion/BloodSimulatedReview-20260920-202844-e81788ad/blood-age-{120,300,600}.png`. All three images were inspected. At the first sample all eight effects are visible. Red effects read as red, and Eclipse effects read as neutral black without visible red mist. Burst has the largest spray, Low has a much smaller footprint, and the effects fade substantially by 0.6 seconds. The blank gray studio background does not establish visibility against dark Aurelion architecture, native hit placement, camera occlusion, crowded combat or GPU cost.

The initial `BloodRenderedReview-20260920-202655-fc10bd97` captures are rejected: the backdrop was back-facing and the preview did not visibly advance Niagara. The corrected run flips the backdrop and explicitly simulates the components. It exited normally, although its log also contains an editor realtime-override ensure; that is retained as a preview limitation rather than suppressed or treated as a clean runtime pass.

The native blood presenter remains unchanged. Its inspected code selects species, size and reduced-effects variants from resolved health damage, with an eight-second cleanup watchdog. This studio did not issue native damage receipts and does not independently verify that runtime path.

The initial full gate `Saved/Validation/20260920-202538-6a64fbc6` is rejected for source integrity because this review script was added during validation. Build and automation processes exited zero, but that run does not qualify the final files. Replacement gate `Saved/Validation/20260920-203146-e9e2030a` passed the full build, all 722 matching automation tests, coverage and source integrity with the final review script unchanged. These tests do not replace the visual review or qualify packaged performance.
