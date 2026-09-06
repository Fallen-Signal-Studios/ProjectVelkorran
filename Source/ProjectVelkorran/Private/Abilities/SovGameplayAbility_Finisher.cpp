// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Abilities/SovGameplayAbility_Finisher.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Combat/SovFinisherPolicy.h"
#include "Combat/SovFinisherTargetComponent.h"
#include "Combat/SovNativeDamageReceipt.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GAS/NarrativeDamageExecCalc.h"
#include "GameFramework/Character.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "UObject/StrongObjectPtr.h"
#include "Misc/ScopeExit.h"
USovGameplayEffect_FinisherProtection::USovGameplayEffect_FinisherProtection()
{ DurationPolicy=EGameplayEffectDurationType::HasDuration; DurationMagnitude=FGameplayEffectModifierMagnitude(FScalableFloat(2.f)); StackingType=EGameplayEffectStackingType::None; }
USovGameplayEffect_FinisherDamage::USovGameplayEffect_FinisherDamage()
{
    DurationPolicy=EGameplayEffectDurationType::Instant;
    FGameplayEffectExecutionDefinition Execution; Execution.CalculationClass=UNarrativeDamageExecCalc::StaticClass(); Executions.Add(Execution);
}
USovGameplayAbility_Finisher::USovGameplayAbility_Finisher()
{
    InstancingPolicy=EGameplayAbilityInstancingPolicy::InstancedPerActor; NetExecutionPolicy=EGameplayAbilityNetExecutionPolicy::ServerOnly;
    bRequiresAmmo=false; bBotSelectionEnabled=false;
    const auto& T=FSovGameplayTags::Get(); const auto& N=FNarrativeGameplayTags::Get();
    FGameplayTagContainer AssetTags; AssetTags.AddTag(T.Ability_Finisher); SetAssetTags(AssetTags); InputTag=T.Input_Finisher;
    ActivationBlockedTags.AddTag(N.State_Busy); ActivationBlockedTags.AddTag(N.State_IsDead);
    ActivationBlockedTags.AddTag(N.State_SequencerControlled); ActivationBlockedTags.AddTag(N.State_Interacting);
    ActivationBlockedTags.AddTag(T.State_Fatal); ActivationBlockedTags.AddTag(T.State_Poise_Broken);
    ActivationBlockedTags.AddTag(T.State_Guarding); ActivationBlockedTags.AddTag(T.State_Deflecting);
    ActivationOwnedTags.AddTag(T.State_Finisher_Active); ActivationOwnedTags.AddTag(N.State_Busy);
}
bool USovGameplayAbility_Finisher::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
    const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* Relevant) const
{
    return FMath::IsFinite(SequenceDuration) && SequenceDuration>=.8f && SequenceDuration<=1.8f
        && FMath::IsFinite(MaximumDistance) && MaximumDistance>=50.f && MaximumDistance<=300.f
        && FMath::IsFinite(MaximumFacingAngle) && MaximumFacingAngle>=0.f && MaximumFacingAngle<=75.f
        && FMath::IsFinite(FallbackStrikeDamage) && FallbackStrikeDamage>=0.f
        && Info && Info->IsNetAuthority() && Info->AvatarActor.IsValid() && Info->AbilitySystemComponent.IsValid()
        && Info->AbilitySystemComponent->GetAvatarActor()==Info->AvatarActor.Get()
        && Info->AbilitySystemComponent->HasMatchingGameplayTag(FSovGameplayTags::Get().Character_Player)
        && Super::CanActivateAbility(Handle,Info,SourceTags,TargetTags,Relevant);
}
bool USovGameplayAbility_Finisher::SourceValid() const
{
    return IsActive() && ActionAvatar.IsValid() && ActionASC.IsValid() && !ActionAvatar->IsActorBeingDestroyed()
        && CurrentActorInfo && CurrentActorInfo->IsNetAuthority() && CurrentActorInfo->AvatarActor.Get()==ActionAvatar.Get()
        && CurrentActorInfo->AbilitySystemComponent.Get()==ActionASC.Get() && ActionASC->GetAvatarActor()==ActionAvatar.Get()
        && UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(ActionAvatar.Get())==ActionASC.Get()
        && !ActionASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Fatal)
        && !ActionASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_IsDead)
        && !ActionASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_SequencerControlled)
        && ActionASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute())>0.f;
}
bool USovGameplayAbility_Finisher::OwnsAction(const FGuid& ExpectedLease) const
{ return ExpectedLease.IsValid() && Lease==ExpectedLease && SourceValid() && TargetComponent && TargetComponent->OwnsLease(this,ExpectedLease); }
void USovGameplayAbility_Finisher::FinishReservedAction(const FGuid& ExpectedLease)
{ if (Lease==ExpectedLease && IsActive()) { FinishAction(); } }
bool USovGameplayAbility_Finisher::TargetInReach(AActor* Target) const
{
    AActor* Source=GetAvatarActorFromActorInfo();
    if (!IsValid(Source) || !IsValid(Target) || Source==Target || !Source->GetWorld()) { return false; }
    const FVector Offset=Target->GetActorLocation()-Source->GetActorLocation();
    if (Offset.ContainsNaN() || Offset.SizeSquared()>FMath::Square(FMath::Clamp(MaximumDistance,50.f,300.f))
        || FVector::DotProduct(Source->GetActorForwardVector().GetSafeNormal2D(),Offset.GetSafeNormal2D())<FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(MaximumFacingAngle,0.f,75.f)))) { return false; }
    FCollisionQueryParams Params(SCENE_QUERY_STAT(SovFinisherLOS),false,Source);
    TArray<AActor*> Attached; Source->GetAttachedActors(Attached,true,true); Params.AddIgnoredActors(Attached);
    FHitResult Hit;
    return !Source->GetWorld()->LineTraceSingleByChannel(Hit,Source->GetActorLocation(),Target->GetActorLocation(),ECC_Visibility,Params)
        || Hit.GetActor()==Target || (Hit.GetActor() && Hit.GetActor()->IsOwnedBy(Target));
}
AActor* USovGameplayAbility_Finisher::FindFinisherTarget() const
{
    AActor* Source=GetAvatarActorFromActorInfo(); if (!IsValid(Source) || !Source->HasAuthority() || !Source->GetWorld()) { return nullptr; }
    TArray<FOverlapResult> Hits; FCollisionObjectQueryParams Objects; Objects.AddObjectTypesToQuery(ECC_Pawn);
    Source->GetWorld()->OverlapMultiByObjectType(Hits,Source->GetActorLocation(),FQuat::Identity,Objects,
        FCollisionShape::MakeSphere(FMath::Clamp(MaximumDistance,50.f,300.f)),FCollisionQueryParams(SCENE_QUERY_STAT(SovFinisherFind),false,Source));
    AActor* Best=nullptr; float BestDistance=TNumericLimits<float>::Max();
    for (const FOverlapResult& Hit:Hits)
    {
        AActor* Candidate=Hit.GetActor(); auto* Component=IsValid(Candidate)?Candidate->FindComponentByClass<USovFinisherTargetComponent>():nullptr;
        if (!Component || !Component->IsAvailableFor(Source) || !TargetInReach(Candidate)) { continue; }
        const float Distance=FVector::DistSquared(Source->GetActorLocation(),Candidate->GetActorLocation());
        if (Distance<BestDistance) { Best=Candidate; BestDistance=Distance; }
    }
    return Best;
}
bool USovGameplayAbility_Finisher::AlignmentSafe(AActor* Target) const
{
    const ACharacter* Player=Cast<ACharacter>(ActionAvatar.Get()); const ACharacter* Enemy=Cast<ACharacter>(Target);
    UWorld* World=GetWorld(); auto* Nav=World?FNavigationSystem::GetCurrent<UNavigationSystemV1>(World):nullptr;
    if (!Player || !Enemy || !Nav || ExitOffset.ContainsNaN() || ExitOffset.Size()>100.f || !FinisherMontage
        || !TargetInReach(Target) || FMath::Abs(Player->GetActorLocation().Z-Enemy->GetActorLocation().Z)>60.f) { return false; }
    const FVector Exit=Player->GetActorLocation()+Player->GetActorRotation().RotateVector(ExitOffset);
    FNavLocation PlayerNav, EnemyNav, ExitNav;
    if (!Nav->ProjectPointToNavigation(Player->GetActorLocation(),PlayerNav,FVector(40.f,40.f,150.f))
        || !Nav->ProjectPointToNavigation(Enemy->GetActorLocation(),EnemyNav,FVector(40.f,40.f,150.f))
        || !Nav->ProjectPointToNavigation(Exit,ExitNav,FVector(40.f,40.f,150.f))) { return false; }
    UNavigationPath* Path=UNavigationSystemV1::FindPathToLocationSynchronously(World,PlayerNav.Location,EnemyNav.Location);
    if (!Path || !Path->IsValid() || Path->IsPartial() || Path->GetPathLength()>MaximumDistance*1.5f) { return false; }
    FCollisionQueryParams Params(SCENE_QUERY_STAT(SovFinisherExit),false,Player);
    const auto* Capsule=Player->GetCapsuleComponent();
    return !World->OverlapBlockingTestByChannel(Exit,FQuat::Identity,Capsule->GetCollisionObjectType(),
        FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()),Params);
}
bool USovGameplayAbility_Finisher::ApplyProtection()
{
    const auto& T=FSovGameplayTags::Get(); const auto& N=FNarrativeGameplayTags::Get();
    const FGuid ExpectedLease=Lease;
    const TWeakObjectPtr<UAbilitySystemComponent> Source=ActionASC, Target=TargetASC;
    const float Duration=static_cast<float>(SovFinisher::Duration(SequenceDuration,bAligned))+.2f;
    const auto Apply=[Source,Duration,&T,&N](UAbilitySystemComponent* ASC,bool bTarget)
    {
        FGameplayEffectSpec Spec(GetDefault<USovGameplayEffect_FinisherProtection>(),Source->MakeEffectContext(),1.f);
        Spec.DynamicGrantedTags.AddTag(T.State_InterruptProtected); Spec.DynamicGrantedTags.AddTag(N.State_Movement_Lock);
        if (bTarget) { Spec.DynamicGrantedTags.AddTag(T.State_Finisher_Target); Spec.DynamicGrantedTags.AddTag(N.State_Busy); }
        Spec.SetDuration(Duration,true);
        return Source->ApplyGameplayEffectSpecToTarget(Spec,ASC);
    };
    const auto PlayerHandle=Apply(Source.Get(),false);
    if (!OwnsAction(ExpectedLease) || !Target.IsValid())
    { if (Source.IsValid() && PlayerHandle.IsValid()) { Source->RemoveActiveGameplayEffect(PlayerHandle); } return false; }
    PlayerProtection=PlayerHandle;
    const auto TargetHandle=Apply(Target.Get(),true);
    if (!OwnsAction(ExpectedLease))
    { if (Target.IsValid() && TargetHandle.IsValid()) { Target->RemoveActiveGameplayEffect(TargetHandle); } return false; }
    TargetProtection=TargetHandle;
    return PlayerProtection.IsValid() && TargetProtection.IsValid();
}
void USovGameplayAbility_Finisher::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
    const FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event)
{
    Super::ActivateAbility(Handle,Info,Activation,Event);
    if (!IsActive() || !Info || !Info->IsNetAuthority()) { return; }
    ActionAvatar=Info->AvatarActor; ActionASC=Info->AbilitySystemComponent; bStruck=false; bAligned=false;
    AActor* Target=Event && Event->Target ? const_cast<AActor*>(Event->Target.Get()) : FindFinisherTarget();
    TargetComponent=IsValid(Target)?Target->FindComponentByClass<USovFinisherTargetComponent>():nullptr;
    TargetASC=UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
    if (!SourceValid() || !TargetComponent || !TargetASC.IsValid() || !TargetInReach(Target)
        || !TargetComponent->Reserve(this,ActionAvatar.Get(),Lease)) { FinishAction(); return; }
    const FGuid ExpectedLease=Lease;
    bAligned=AlignmentSafe(Target);
    if (!CommitAbility(Handle,Info,Activation) || !OwnsAction(ExpectedLease)
        || !ApplyProtection()) { FinishReservedAction(ExpectedLease); return; }
    // Only cancel live combat actions on the reserved opponent; unrelated passives remain granted.
    TArray<FGameplayAbilitySpecHandle> Attacks;
    for (const FGameplayAbilitySpec& Spec:TargetASC->GetActivatableAbilities())
    { if (Spec.IsActive() && Cast<UNarrativeCombatAbility>(Spec.Ability)) { Attacks.Add(Spec.Handle); } }
    for (const auto& Attack:Attacks)
    {
        TargetASC->CancelAbilityHandle(Attack);
        if (!OwnsAction(ExpectedLease)) { FinishReservedAction(ExpectedLease); return; }
    }
    const float Duration=static_cast<float>(SovFinisher::Duration(SequenceDuration,bAligned));
    GetWorld()->GetTimerManager().SetTimer(StrikeTimer,this,&ThisClass::Strike,Duration-.2f,false);
    GetWorld()->GetTimerManager().SetTimer(FinishTimer,this,&ThisClass::FinishAction,Duration,false);
    GetWorld()->GetTimerManager().SetTimer(CheckTimer,this,&ThisClass::CheckAction,.025f,true);
    StrikeTask=UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this,FSovGameplayTags::Get().Event_Finisher_Strike,nullptr,true,true);
    StrikeTask->EventReceived.AddDynamic(this,&ThisClass::OnStrikeEvent); StrikeTask->ReadyForActivation();
    if (bAligned)
    {
        const float Rate=FMath::Max(FinisherMontage->GetPlayLength()/Duration,.01f);
        const float MontageResult=ActionASC->PlayMontage(this,Activation,FinisherMontage,Rate,MontageSection);
        if (!OwnsAction(ExpectedLease)) { return; }
        if (MontageResult<=0.f)
        {
            bAligned=false;
            GetWorld()->GetTimerManager().SetTimer(StrikeTimer,this,&ThisClass::Strike,.15f,false);
            GetWorld()->GetTimerManager().SetTimer(FinishTimer,this,&ThisClass::FinishAction,.35f,false);
        }
    }
    if (OwnsAction(ExpectedLease)) { OnFinisherStarted(Target,bAligned); }
}
void USovGameplayAbility_Finisher::OnStrikeEvent(FGameplayEventData Payload)
{ if (Payload.Instigator.Get()==ActionAvatar.Get() && (!Payload.Target || Payload.Target.Get()==(TargetComponent?TargetComponent->GetOwner():nullptr))) { Strike(); } }
void USovGameplayAbility_Finisher::CheckAction()
{
    if (!SourceValid() || !TargetComponent || !TargetASC.IsValid() || TargetASC->GetAvatarActor()!=TargetComponent->GetOwner()
        || !TargetComponent->OwnsLease(this,Lease)
        || (!bStruck && !TargetComponent->IsReservedTargetValid(this,Lease,ActionAvatar.Get()))
        || (!bStruck && !TargetInReach(TargetComponent->GetOwner()))) { FinishAction(); }
}
void USovGameplayAbility_Finisher::Strike()
{
    if (bStruck) { return; }
    const FGuid CapturedLease=Lease; CheckAction();
    if (!OwnsAction(CapturedLease)) { return; }
    bStruck=true;
    AActor* Target=TargetComponent->GetOwner(); const auto& T=FSovGameplayTags::Get();
    const bool bNormal=TargetComponent->TargetKind==ESovFinisherTargetKind::Normal && !TargetASC->HasMatchingGameplayTag(T.Character_Enemy_Boss);
    const float Damage=static_cast<float>(SovFinisher::StrikeDamage(TargetASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()),
        bAligned?TargetComponent->PhaseDamage:FallbackStrikeDamage,bNormal,bAligned));
    FGameplayEffectContextHandle Context=ActionASC->MakeEffectContext(); Context.AddInstigator(ActionAvatar.Get(),ActionAvatar.Get()); Context.AddSourceObject(this);
    FGameplayEffectSpec Spec(GetDefault<USovGameplayEffect_FinisherDamage>(),Context,GetAbilityLevel());
    Spec.AddDynamicAssetTag(T.Ability_Finisher); Spec.AddDynamicAssetTag(T.Damage_Channel_Edge); Spec.AddDynamicAssetTag(T.Damage_AlreadyResolved);
    Spec.AddDynamicAssetTag(T.Damage_BypassGuard); Spec.AddDynamicAssetTag(T.Damage_BypassDeflection); Spec.AddDynamicAssetTag(T.Damage_BypassShield);
    Spec.SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Damage,Damage);
    Spec.SetSetByCallerMagnitude(T.SetByCaller_Damage_PoiseDamage,0.f);
    if (UNarrativeDamageExecCalc::ShouldRejectTransaction(ActionASC.Get(),TargetASC.Get(),Spec)) { FinishAction(); return; }
    TStrongObjectPtr<AActor> KeepTarget(Target);
    TStrongObjectPtr<USovFinisherTargetComponent> OutcomeOwner(TargetComponent.Get());
    TStrongObjectPtr<UNarrativeAbilitySystemComponent> DamageTarget(Cast<UNarrativeAbilitySystemComponent>(TargetASC.Get()));
    TStrongObjectPtr<UAbilitySystemComponent> DamageSource(ActionASC.Get());
    if (!DamageTarget.IsValid() || Damage <= 0.f) { FinishReservedAction(CapturedLease); return; }
    TStrongObjectPtr<const UNarrativeAttributeSetBase> TargetAttributes(DamageTarget->GetSet<UNarrativeAttributeSetBase>());
    if (!TargetAttributes.IsValid()) { FinishReservedAction(CapturedLease); return; }
    const uint64 TargetActorInfoEpoch=DamageTarget->GetCombatActorInfoEpoch();
    const int32 TargetReadyEpoch=DamageTarget->GetCharacterReadyEpoch();
    const uint64 TargetLifeEpoch=TargetAttributes->GetCombatLifeEpoch();
    const FGameplayTag ResolvedPhase=OutcomeOwner->RequiredPhaseTag;
    FGameplayEventData PhasePayload;
    PhasePayload.EventTag=T.Event_Finisher_PhaseResolved;
    PhasePayload.Instigator=ActionAvatar.Get(); PhasePayload.Target=Target;
    if (ResolvedPhase.IsValid()) { PhasePayload.TargetTags.AddTag(ResolvedPhase); }
    PhasePayload.ContextHandle=Context;
    const bool bWantsPhaseOutcome=!bNormal && bAligned;
    FGuid PendingOutcome;
    if (bWantsPhaseOutcome && !OutcomeOwner->BeginPhaseOutcome(this,CapturedLease,PendingOutcome))
    { FinishReservedAction(CapturedLease); return; }
    ON_SCOPE_EXIT
    {
        if (IsValid(OutcomeOwner.Get())) { OutcomeOwner->FinishPhaseOutcome(PendingOutcome); }
    };
    TStrongObjectPtr<USovNativeDamageReceipt> Receipt(NewObject<USovNativeDamageReceipt>());
    Receipt->ExpectedTarget=Target; Receipt->ExpectedContext=Context.Get(); Receipt->bRequireNativeProof=true;
    // Target publication survives source ability cancellation, while native proof
    // rejects rejected/zero packets and retired target lives. The receipt is local
    // to this exact synchronous execution, never an ability-owned mutable member.
    DamageTarget->OnDamageResolvedAsTarget.AddDynamic(Receipt.Get(), &USovNativeDamageReceipt::ReceiveResult);
    DamageSource->ApplyGameplayEffectSpecToTarget(Spec,DamageTarget.Get());
    if (IsValid(DamageTarget.Get()))
    { DamageTarget->OnDamageResolvedAsTarget.RemoveDynamic(Receipt.Get(), &USovNativeDamageReceipt::ReceiveResult); }
    const bool bSameTarget=IsValid(Target) && !Target->IsActorBeingDestroyed()
        && IsValid(OutcomeOwner.Get()) && Target->FindComponentByClass<USovFinisherTargetComponent>()==OutcomeOwner.Get()
        && IsValid(DamageTarget.Get()) && DamageTarget->GetAvatarActor()==Target
        && UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target)==DamageTarget.Get()
        && DamageTarget->GetCombatActorInfoEpoch()==TargetActorInfoEpoch
        && DamageTarget->GetCharacterReadyEpoch()==TargetReadyEpoch
        && DamageTarget->GetSet<UNarrativeAttributeSetBase>()==TargetAttributes.Get()
        && TargetAttributes->GetCombatLifeEpoch()==TargetLifeEpoch;
    const bool bPhaseOutcome=bWantsPhaseOutcome && IsValid(OutcomeOwner.Get())
        && OutcomeOwner->CommitPhaseOutcome(PendingOutcome,ResolvedPhase,Receipt->bAppliedDamage,bSameTarget);
    if (bPhaseOutcome)
    { UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Target,PhasePayload.EventTag,PhasePayload); }
    if (Receipt->bAppliedDamage && bSameTarget && OwnsAction(CapturedLease)) { OnFinisherResolved(Target,bPhaseOutcome); }

}
void USovGameplayAbility_Finisher::FinishAction()
{ if (IsActive()) { EndAbility(CurrentSpecHandle,CurrentActorInfo,CurrentActivationInfo,true,!bStruck); } }
void USovGameplayAbility_Finisher::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
    const FGameplayAbilityActivationInfo Activation, bool bReplicate, bool bCancelled)
{
    if (!IsEndAbilityValid(Handle,Info)) { return; }
    if (GetWorld()) { auto& Timers=GetWorld()->GetTimerManager(); Timers.ClearTimer(StrikeTimer); Timers.ClearTimer(FinishTimer); Timers.ClearTimer(CheckTimer); }
    const FGuid OldLease=Lease; Lease.Invalidate();
    if (TargetComponent) { TargetComponent->Release(this,OldLease); }
    const auto OldPlayer=ActionASC; const auto OldTarget=TargetASC;
    const auto PlayerHandle=PlayerProtection, TargetHandle=TargetProtection; PlayerProtection.Invalidate(); TargetProtection.Invalidate();
    ActionAvatar.Reset(); ActionASC.Reset(); TargetASC.Reset(); TargetComponent=nullptr;
    if (OldPlayer.IsValid())
    {
        if (OldPlayer->GetAnimatingAbility()==this) { OldPlayer->CurrentMontageStop(.1f); }
        if (PlayerHandle.IsValid()) { OldPlayer->RemoveActiveGameplayEffect(PlayerHandle); }
    }
    if (OldTarget.IsValid() && TargetHandle.IsValid()) { OldTarget->RemoveActiveGameplayEffect(TargetHandle); }
    Super::EndAbility(Handle,Info,Activation,bReplicate,bCancelled);
}
