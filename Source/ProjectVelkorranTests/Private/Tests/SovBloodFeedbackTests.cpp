#include "Presentation/SovBloodFeedbackComponent.h"
#include "AI/SovAurelionEnemyRoles.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovBloodFeedbackPolicyTest, "ProjectVelkorran.Presentation.Blood.HealthDamageAndSpecies",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovBloodFeedbackPolicyTest::RunTest(const FString& Parameters)
{
    FSovDamageResult R;
    R.AppliedShieldDamage = 30.f;
    TestFalse(TEXT("Shield damage never bleeds"), USovBloodFeedbackComponent::ShouldPresent(R));
    R.AppliedHealthDamage = 10.f;
    TestTrue(TEXT("Applied flesh damage bleeds"), USovBloodFeedbackComponent::ShouldPresent(R));
    R.bPeriodicDamage = true;
    TestFalse(TEXT("Periodic ticks do not spray"), USovBloodFeedbackComponent::ShouldPresent(R));
    R.bPeriodicDamage = false;
    R.AppliedHealthDamage = 0.f;
    TestFalse(TEXT("Zero health damage never bleeds"), USovBloodFeedbackComponent::ShouldPresent(R));
    for (const UClass* Class : {ASovAurelionLinkbound::StaticClass(), ASovAurelionWallRunner::StaticClass(),
        ASovAurelionWeaver::StaticClass(), ASovAurelionElite::StaticClass()})
    {
        const auto* Blood = CastChecked<AActor>(Class->GetDefaultObject())->FindComponentByClass<USovBloodFeedbackComponent>();
        TestTrue(*Class->GetName(), Blood && Blood->bEnabled && Blood->bBlackBlood);
    }
    const auto* Drone = GetDefault<ASovAurelionSecurityDrone>()->FindComponentByClass<USovBloodFeedbackComponent>();
    TestTrue(TEXT("Mechanical drone has no flesh blood"), Drone && !Drone->bEnabled);
    const auto* Player = GetDefault<ASovPlayerCharacterBase>()->FindComponentByClass<USovBloodFeedbackComponent>();
    TestTrue(TEXT("Protagonist has red blood"), Player && Player->bEnabled && !Player->bBlackBlood);
    return true;
}
#endif
