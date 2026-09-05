// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovPlayerCombatRepairTestFixtures.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Sovereign/SovGameplayTags.h"
#include "UObject/UnrealType.h"
USovRepairAmmo::USovRepairAmmo()
{ bStackable=true; MaxStackSize=1000; Weight=.01f; }
USovRepairWeapon::USovRepairWeapon()
{
    RequiredAmmo=USovRepairAmmo::StaticClass(); ClipSize=1; bBotsConsumeAmmo=true; Weight=1.f;
    RecoilImpulseTranslationMin=RecoilImpulseTranslationMax=FVector(2,0,0);
    HipRecoilImpulseTranslationMin=HipRecoilImpulseTranslationMax=FVector(7,0,0);
}
USovRepairEchoAbility::USovRepairEchoAbility()
{
    NetExecutionPolicy=EGameplayAbilityNetExecutionPolicy::ServerOnly;
    bRequiresAllowedWeapon=false; MinimumEchoRequired=20.f; EchoCost=20.f;
    EchoSpendTag=FSovGameplayTags::Get().Ability_Echo_Tarrik_CinderJudgement;
    MaximumActiveDuration=1.f;
}
void USovRepairEchoAbility::ProcessEvent(UFunction* Function,void* Parameters)
{
    if (Function && Function->GetFName()==TEXT("ReceiveEchoAbilityStarted")) { ++StartedCount; }
    Super::ProcessEvent(Function,Parameters);
}
USovRepairJudgementAbility::USovRepairJudgementAbility()
{
    AllowedWeaponClasses.Add(UWeaponItem::StaticClass()); bAutoReleasePayload=false;
    FallbackMuzzleOffset=FVector(100,0,0); MaximumRange=2000.f; TraceRadius=0.f;
    DirectDamage=10.f; DirectPoiseDamage=0.f; DirectShieldCoefficient=1.f;
    ExplosionDamage=10.f; ExplosionPoiseDamage=0.f; ExplosionShieldCoefficient=1.f;
    ExplosionRadius=300.f; PostReleaseRecovery=.1f; bApplyExplosionPhysicsImpulse=false;
}
void USovPlayerCombatRepairProbe::DuringAmmoMutation()
{
    if (!bArmed || !Weapon) { return; } bArmed=false;
    bNestedConsumeAccepted=Weapon->ConsumeAmmo(1);
    bNestedReloadAccepted=Weapon->Reload();
}
void USovPlayerCombatRepairProbe::DuringEchoDebit(float OldEcho,float NewEcho,float Maximum)
{
    if (!bArmed || NewEcho>=OldEcho || !ASC) { return; } bArmed=false;
    ASC->CancelAbilityHandle(Handle);
}
void USovPlayerCombatRepairProbe::DuringJudgementDamage(const FSovDamageResult& Result)
{
    if (!bArmed || !Judgement || !ASC) { return; } bArmed=false;
    Judgement->FinishEchoAbility(true);
    if (bReactivateJudgement) { bReactivationAccepted=ASC->TryActivateAbility(Handle,false); }
}

ETeamAttitude::Type ASovRepairFinisherCharacter::GetTeamAttitudeTowards(const AActor& Other) const
{
    if (ReenterOnAttitudeCall>0 && --ReenterOnAttitudeCall==0)
    {
        auto* ASC=GetNarrativeAbilitySystemComponent();
        ASC->CancelAbilityHandle(FinisherHandle);
        bRestarted=ASC->TryActivateAbility(FinisherHandle,false);
    }
    return Super::GetTeamAttitudeTowards(Other);
}
void USovRepairFinisherAbility::UnlockEndForTest()
{
    check(ScopeLockCount>0); --ScopeLockCount;
    if (ScopeLockCount==0)
    {
        auto Pending=MoveTemp(WaitingToExecute);
        for (auto& Call:Pending) { Call.ExecuteIfBound(); }
    }
}
