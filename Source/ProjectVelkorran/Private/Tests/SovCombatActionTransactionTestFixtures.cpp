// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovCombatActionTransactionTestFixtures.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Sovereign/SovGameplayTags.h"
#include "UObject/Class.h"

USovCombatActionTransactionEchoAbility::USovCombatActionTransactionEchoAbility()
{
    NetExecutionPolicy=EGameplayAbilityNetExecutionPolicy::ServerOnly;
    bRequiresAllowedWeapon=false;
    MinimumEchoRequired=20.f;
    EchoCost=20.f;
    RequiredCharacterTag=FSovGameplayTags::Get().Character_Player_Selene;
    EchoSpendTag=FSovGameplayTags::Get().Ability_Echo_Selene_AxiomNullPulse;
    MaximumActiveDuration=1.f;
}

void USovCombatActionTransactionEchoAbility::ProcessEvent(UFunction* Function,void* Parameters)
{
    if (Function)
    {
        const FName Name=Function->GetFName();
        if (Name==TEXT("ReceiveEchoAbilityStarted"))
        {
            ++StartedCount;
            if (bRestartDuringStarted)
            {
                bRestartDuringStarted=false;
                auto* ASC=GetAbilitySystemComponentFromActorInfo();
                const auto Handle=CurrentSpecHandle;
                FinishEchoAbility(true);
                bRestartAccepted=ASC&&ASC->TryActivateAbility(Handle,false);
            }
        }
        else if (Name==TEXT("ReceiveEchoAbilityAuthorityCommitted")) { ++CommittedCount; }
        else if (Name==TEXT("ReceiveEchoAbilityEnded")) { ++EndedCount; }
    }
    Super::ProcessEvent(Function,Parameters);
}

void USovCombatActionTransactionEchoAbility::UnlockEndForTest()
{
    check(ScopeLockCount>0);
    --ScopeLockCount;
    if (ScopeLockCount==0)
    {
        auto Pending=MoveTemp(WaitingToExecute);
        for (auto& Call:Pending) { Call.ExecuteIfBound(); }
    }
}

void USovCombatActionTransactionProbe::DuringEchoDebit(float OldEcho,float NewEcho,float Maximum)
{
    if (!bArmed||!ASC||NewEcho>=OldEcho) { return; }
    bArmed=false;
    if ((Mutation==ESovCombatActionDebitMutation::ReplaceAvatar || Mutation==ESovCombatActionDebitMutation::RestoreOriginalAvatar)
        && ReplacementAvatar)
    {
        AActor* OriginalOwner=ASC->GetOwnerActor(); AActor* OriginalAvatar=ASC->GetAvatarActor();
        ASC->InitAbilityActorInfo(ReplacementAvatar,ReplacementAvatar);
        if (Mutation==ESovCombatActionDebitMutation::RestoreOriginalAvatar)
        { ASC->InitAbilityActorInfo(OriginalOwner,OriginalAvatar); }
    }
    else if (Mutation==ESovCombatActionDebitMutation::Freeze)
    { ASC->AddLooseGameplayTag(FSovGameplayTags::Get().State_Status_Frozen); }
    else if (Mutation==ESovCombatActionDebitMutation::ZeroHealth || Mutation==ESovCombatActionDebitMutation::RestoreLife)
    {
        ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(),0.f);
        if (Mutation==ESovCombatActionDebitMutation::RestoreLife)
        { ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(),100.f); }
    }
    else { ASC->CancelAbilityHandle(Handle); }
}

USkeletalMeshComponent* USovCombatActionTransactionMeleeAbility::ResolveMeleeTraceMesh_Implementation() const
{
    if (bRestartDuringMesh)
    {
        bRestartDuringMesh=false;
        auto* ASC=GetAbilitySystemComponentFromActorInfo();
        const auto Handle=CurrentSpecHandle;
        if (ASC)
        {
            ASC->CancelAbilityHandle(Handle);
            bRestartAccepted=ASC->TryActivateAbility(Handle,false);
        }
    }
    return Super::ResolveMeleeTraceMesh_Implementation();
}

void USovCombatActionTransactionMeleeAbility::ProcessEvent(UFunction* Function,void* Parameters)
{
    if (Function&&Function->GetFName()==TEXT("OnMeleeNodeStarted")) { ++NodeStartedCount; }
    Super::ProcessEvent(Function,Parameters);
}

ETeamAttitude::Type ASovCombatActionTransactionTeamCharacter::GetTeamAttitudeTowards(const AActor& Other) const
{
    if (&Other!=this)
    {
        bQueriedAttitude=true;
        if (bCancelDuringAttitude)
        {
            bCancelDuringAttitude=false;
            GetNarrativeAbilitySystemComponent()->CancelAbilityHandle(Handle);
        }
    }
    return Super::GetTeamAttitudeTowards(Other);
}
