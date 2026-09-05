# Campaign engineering validation record

Date: 4 September 2026. Scope: the campaign engineering changes based on Axiom commit `8121e30a5374473ac30e027403b7e1d61dbae77a`. See [CampaignEngineeringDelivery.md](CampaignEngineeringDelivery.md) for delivered behavior and remaining limits, and [CampaignEngineeringFiles.md](CampaignEngineeringFiles.md) for the exact file inventory.

## Executed successfully

Command: `python Scripts/Test-NativePolicies.py`.

Compiler: `g++ (Ubuntu 13.3.0-6ubuntu2~24.04) 13.3.0`. Every suite compiles with C++17, `-Wall -Wextra -Werror -pedantic -fsanitize=undefined -fno-sanitize-recover=all`, then executes the resulting binary. The runner uses production policy/math headers; it does not imitate Unreal classes.

| Portable suite | Exercised behavior | Result |
|---|---|---|
| `Scripts/Tests/AxiomPulseMathTests.cpp` | Charge and cone geometry; 34 checks | Pass |
| `Scripts/Tests/CampaignPolicyTests.cpp` | Mission/beat admission and skip policy; 24 assertions | Pass |
| `Scripts/Tests/CombatTransactionPolicyTests.cpp` | Damage, pure-Poise and control transaction admission; 14 assertions | Pass |
| `Scripts/Tests/EchoAwardPolicyTests.cpp` | Typed award eligibility and exclusions | Pass |
| `Tests/Portable/SovCinderLineMathTests.cpp` | Persistent lane timing and bounds; 2,102 checks | Pass |
| `Tests/Portable/SovCorruptionMathTests.cpp` | Exposure integration, bands and hysteresis | Pass |
| `Tests/Portable/SovEncounterPolicyTests.cpp` | State/load/reward policy, invalid values and 42,000 resource cases | Pass |
| `Tests/Portable/SovProtectionAwardPolicyTests.cpp` | Cooldown boundaries, nonfinite values and clock behavior; 13 assertions | Pass |
| `Tests/Portable/SovSelenePayloadMathTests.cpp` | Field/lane/projectile numeric boundaries | Pass |
| `Tests/Portable/SovTechniquePolicyTests.cpp` | Point budget and ownership policy; 3,500 checks | Pass |

Final runner result: `PASS: 10 portable suites. Unreal build/automation not run.` No compiler error, assertion failure or undefined-behavior sanitizer failure was reported.

Source review also covered shared ownership across avatar swaps, deferred mission restore, transactional save output, grant cleanup, damage receipt identity, ability cancellation and companion arrival. These reviews and whitespace/header consistency checks are source evidence, not substitute build results.

## Authored but not executed

The `ProjectVelkorran.Campaign` Unreal automation namespace contains existing and new World/GAS/serialization/AI tests, including Axiom, bot attack selection, defense routing, Guard lifecycle, both protagonists' payloads, Echo rewards, protection intercepts, encounter recovery, campaign state, managed handoff, travel failure, Techniques, corruption and co-action. Module documents list individual registrations and content prerequisites.

No UHT, Unreal C++ compilation, Unreal automation, Blueprint compilation, commandlet execution, cook, packaged build, PIE playthrough, frame-rate comparison or performance measurement ran here. The environment has no `UnrealEditor`, `UnrealEditor-Cmd`, `UnrealBuildTool`, `dotnet` or `pwsh`. Project and Narrative content contain zero available `.uasset`/`.umap` files, and the enabled ZenDyn plugin is absent.

Consequently there is no Unreal build result to label as passing and no runtime certification that Level 1 or Level 2 plays end to end. Runtime fixtures themselves may expose engine/API or configuration errors when first compiled; those errors remain work to fix, not an accepted waiver.

## Required engine acceptance

1. Restore UE5.7, the enabled plugins and the complete game/Narrative content. Run `Scripts/Validate-Unreal.ps1` as documented in [UnrealValidation.md](UnrealValidation.md), building before running the entire campaign namespace.
2. Compile all migrated Blueprints. Ensure presentation graphs do not also produce damage/rewards now handled natively. Run the mission-manifest preflight described in [CampaignHandoff.md](CampaignHandoff.md).
3. Exercise every approved weapon/Echo loop, defense and enemy kit in the greybox. Include cancellation, ownership changes, depleted resources, immunity, cover and drop/reward replay cases from the module validation documents.
4. Record a clean-boot M01-to-M02 playthrough, then repeat death/retry and disk load at each critical beat, including Lyric's arrival and extraction, authored cinematic skip and save-write failure.
5. Run cook/package and target-hardware presentation, accessibility and performance acceptance. Keep this source pass in draft review until its required native and content gates have evidence.
