// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Melee/SovGameplayAbility_Melee.h"
#include "Melee/SovMeleeAttackDefinition.h"
#include "Melee/SovAbilityTask_MeleeSweep.h"
#include "Melee/SovMeleePolicy.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "ArsenalStatics.h"
#include "Combat/SovEchoAttackReceipt.h"
#include "Combat/SovNativeDamageReceipt.h"
#include "Combat/SovSelenePayload.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Exertion/SovGameplayAbility_Exertion.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GAS/NarrativeDamageExecCalc.h"
#include "Items/WeaponItem.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "Targeting/SovTargetingComponent.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "UnrealFramework/NarrativeGameUserSettings.h"
#include "UObject/StrongObjectPtr.h"
#include "Weapons/WeaponVisual.h"
USovGameplayEffect_MeleeDamage::USovGameplayEffect_MeleeDamage()
{
    DurationPolicy=EGameplayEffectDurationType::Instant; FGameplayEffectExecutionDefinition Execution;
    Execution.CalculationClass=UNarrativeDamageExecCalc::StaticClass(); Executions.Add(Execution);
}
USovGameplayAbility_Melee::USovGameplayAbility_Melee()
{
    InstancingPolicy=EGameplayAbilityInstancingPolicy::InstancedPerActor; NetExecutionPolicy=EGameplayAbilityNetExecutionPolicy::ServerOnly;
    bRequiresAmmo=false; DefaultBotAttackRange=180.f;
    const auto& N=FNarrativeGameplayTags::Get(); const auto& T=FSovGameplayTags::Get();
    SetAssetTags(FGameplayTagContainer(N.Ability_MeleeAttack)); InputTag=N.Narrative_Input_Attack;
    ActivationOwnedTags.AddTag(N.State_Busy); ActivationBlockedTags.AddTag(N.State_Busy);
    ActivationBlockedTags.AddTag(N.State_IsDead); ActivationBlockedTags.AddTag(N.State_Interacting);
    ActivationBlockedTags.AddTag(N.State_SequencerControlled); ActivationBlockedTags.AddTag(N.State_Movement_Ragdoll);
    ActivationBlockedTags.AddTag(T.State_Fatal); ActivationBlockedTags.AddTag(T.State_Poise_Broken);
    ActivationBlockedTags.AddTag(T.State_Guarding); ActivationBlockedTags.AddTag(T.State_Deflecting);
}
bool USovGameplayAbility_Melee::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* Info,
    const FGameplayTagContainer* SourceTags,const FGameplayTagContainer* TargetTags,FGameplayTagContainer* Relevant) const
{
    FString Error;
    return Info&&Info->IsNetAuthority()&&Info->AvatarActor.IsValid()&&Info->AbilitySystemComponent.IsValid()
        && AttackDefinition&&AttackDefinition->Validate(Error)&&Super::CanActivateAbility(Handle,Info,SourceTags,TargetTags,Relevant);
}
USkeletalMeshComponent* USovGameplayAbility_Melee::ResolveMeleeTraceMesh_Implementation() const
{
    if (const auto* Visual=GetAbilityWeaponVisual()) { return Visual->WeaponMesh; }
    if (const auto* Character=Cast<ANarrativeCharacter>(GetAvatarActorFromActorInfo())) { return Character->GetMesh(); }
    return nullptr;
}
bool USovGameplayAbility_Melee::ContextValid() const
{
    return IsActive()&&ActionAvatar.IsValid()&&ActionASC.IsValid()&&!ActionAvatar->IsActorBeingDestroyed()
        &&CurrentActorInfo&&CurrentActorInfo->IsNetAuthority()&&CurrentActorInfo->AvatarActor.Get()==ActionAvatar.Get()
        &&CurrentActorInfo->AbilitySystemComponent.Get()==ActionASC.Get()&&ActionASC->GetAvatarActor()==ActionAvatar.Get()
        &&UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(ActionAvatar.Get())==ActionASC.Get()
        &&ActionASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute())>0.f
        &&!ActionASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_IsDead)
        &&!ActionASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Fatal)
        &&!ActionASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Weapon_Equipping)
        &&!ActionASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_SequencerControlled)
        &&!ActionASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Weapon_VerityAbsent)
        &&AttackDefinition&&AttackDefinition->Nodes.IsValidIndex(NodeIndex)
        &&((!bStartedUnarmed&&ActionWeapon.IsValid()&&GetOwnerEquippedWeapon(IsMainhand())==ActionWeapon.Get())
            ||(bStartedUnarmed&&bAllowUnarmed&&!GetOwnerEquippedWeapon(IsMainhand())));
}
bool USovGameplayAbility_Melee::NodeGeometryValid() const
{
    if (!ContextValid()||!ActionMesh.IsValid()||SovSelenePayload::ResolveTarget(ActionMesh->GetOwner())!=ActionAvatar.Get()) { return false; }
    const auto& Node=AttackDefinition->Nodes[NodeIndex];
    return ActionMesh->DoesSocketExist(Node.StartSocket)&&ActionMesh->DoesSocketExist(Node.EndSocket)
        &&FVector::DistSquared(ActionMesh->GetSocketLocation(Node.StartSocket),ActionAvatar->GetActorLocation())<=FMath::Square(450.f)
        &&FVector::DistSquared(ActionMesh->GetSocketLocation(Node.EndSocket),ActionAvatar->GetActorLocation())<=FMath::Square(650.f);
}
bool USovGameplayAbility_Melee::IsCurrentNodeHeavy() const
{
    return ContextValid()&&AttackDefinition->Nodes[NodeIndex].AttackClassifications.HasTagExact(FSovGameplayTags::Get().Damage_Heavy);
}
void USovGameplayAbility_Melee::ActivateAbility(const FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* Info,
    const FGameplayAbilityActivationInfo Activation,const FGameplayEventData* Event)
{
    Super::ActivateAbility(Handle,Info,Activation,Event); if (!IsActive()||!Info||!Info->IsNetAuthority()) { return; }
    ActionASC=Info->AbilitySystemComponent; ActionAvatar=Info->AvatarActor;
    ActionWeapon=GetOwnerEquippedWeapon(IsMainhand()); bStartedUnarmed=!ActionWeapon.IsValid(); ActionMesh=ResolveMeleeTraceMesh(); NodeIndex=0;
    FGuid ExpectedAttack;
    if (!GetSovAttackIdentity(ActionAvatar.Get(),ExpectedAttack)) { FinishMelee(); return; }
    const bool bCommitted=NodeGeometryValid()&&CommitAbility(Handle,Info,Activation);
    FGuid CurrentAttack;
    if (!GetSovAttackIdentity(GetAvatarActorFromActorInfo(),CurrentAttack)||CurrentAttack!=ExpectedAttack) { return; }
    if (!bCommitted||!ContextValid()) { FinishMelee(); return; }
    BeginNode(0);
}
bool USovGameplayAbility_Melee::BeginNode(int32 Index)
{
    if (!ContextValid()||!AttackDefinition->Nodes.IsValidIndex(Index)) { FinishMelee(); return false; }
    if (auto* ASC=Cast<UNarrativeAbilitySystemComponent>(ActionASC.Get())) { ASC->ClearCombatInputWindow(this,InputWindow); }
    InputWindow.Invalidate();
    if (SweepTask) { SweepTask->EndTask(); SweepTask=nullptr; }
    if (Index!=NodeIndex&&!BeginNextSovCombatAttack()) { FinishMelee(); return false; }
    NodeIndex=Index; bHitConfirmed=false; ChargeScalar=1.f;
    if (!NodeGeometryValid()) { FinishMelee(); return false; }
    FGuid ExpectedAttack;
    if (!GetSovAttackIdentity(ActionAvatar.Get(),ExpectedAttack)) { FinishMelee(); return false; }
    const auto& Node=AttackDefinition->Nodes[NodeIndex]; bRequiresChargedRelease=Node.bCharged;
    bCharging=Node.bCharged;
    if (bCharging)
    {
        ChargeStarted=GetWorld()->GetTimeSeconds();
        GetWorld()->GetTimerManager().SetTimer(ChargeTimer,this,&ThisClass::ReleaseCharge,Node.MaximumChargeSeconds,false);
    }
    else { StartAttackWindow(); }
    FGuid CurrentAttack;
    if (!ContextValid()||!GetSovAttackIdentity(ActionAvatar.Get(),CurrentAttack)||CurrentAttack!=ExpectedAttack) { return false; }
    OnMeleeNodeStarted(NodeIndex,bCharging);
    return ContextValid()&&GetSovAttackIdentity(ActionAvatar.Get(),CurrentAttack)&&CurrentAttack==ExpectedAttack;
}
void USovGameplayAbility_Melee::InputReleased(const FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* Info,const FGameplayAbilityActivationInfo Activation)
{
    Super::InputReleased(Handle,Info,Activation); if (IsActive()&&bCharging) { ReleaseCharge(); }
}
void USovGameplayAbility_Melee::ReleaseCharge()
{
    if (!bCharging) { return; } bCharging=false; GetWorld()->GetTimerManager().ClearTimer(ChargeTimer);
    if (!NodeGeometryValid()) { FinishMelee(); return; }
    const auto& Node=AttackDefinition->Nodes[NodeIndex];
    const bool bFull=GetWorld()->GetTimeSeconds()-ChargeStarted>=Node.FullChargeSeconds;
    FGuid ExpectedAttack, CurrentAttack;
    if (!GetSovAttackIdentity(ActionAvatar.Get(),ExpectedAttack)) { FinishMelee(); return; }
    const bool bPaid=TryCommitChargedRelease(bFull?2:0);
    if (!GetSovAttackIdentity(GetAvatarActorFromActorInfo(),CurrentAttack)||CurrentAttack!=ExpectedAttack) { return; }
    if (!bPaid||!NodeGeometryValid()) { FinishMelee(); return; }
    ChargeScalar=bFull?Node.ChargedMultiplier:1.f; StartAttackWindow();
}
void USovGameplayAbility_Melee::ApplyAimCorrection()
{
    const auto* Settings=UNarrativeGameUserSettings::GetSovSettings();
    const float Strength=Settings?Settings->GetMeleeAimAssistStrength():0.f;
    if (Strength<=0.f||!ContextValid()) { return; }
    const auto& Node=AttackDefinition->Nodes[NodeIndex]; const FVector Source=ActionAvatar->GetActorLocation();
    AActor* Best=nullptr; float BestAngle=Node.MaximumAimCorrection;
    TArray<AActor*> Candidates;
    if (const auto* Targeting=ActionAvatar->FindComponentByClass<USovTargetingComponent>())
    { if (AActor* Locked=Targeting->GetLockedTarget()) { Candidates.Add(Locked); } }
    TArray<FOverlapResult> Overlaps; FCollisionObjectQueryParams Objects; Objects.AddObjectTypesToQuery(ECC_Pawn);
    GetWorld()->OverlapMultiByObjectType(Overlaps,Source,FQuat::Identity,Objects,FCollisionShape::MakeSphere(250.f),FCollisionQueryParams(SCENE_QUERY_STAT(SovMeleeAim),false,ActionAvatar.Get()));
    for (const auto& Hit:Overlaps) { if (Hit.GetActor()) { Candidates.AddUnique(Hit.GetActor()); } }
    float DesiredYaw=0.f;
    for (AActor* Candidate:Candidates)
    {
        const auto* ASC=UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Candidate);
        if (!IsValid(Candidate)||!ASC||ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute())<=0.f
            ||UArsenalStatics::GetAttitude(ActionAvatar.Get(),Candidate)!=ETeamAttitude::Hostile
            ||FVector::DistSquared(Source,Candidate->GetActorLocation())>FMath::Square(250.f)) { continue; }
        const float Yaw=FMath::FindDeltaAngleDegrees(ActionAvatar->GetActorRotation().Yaw,(Candidate->GetActorLocation()-Source).Rotation().Yaw);
        if (FMath::Abs(Yaw)>BestAngle) { continue; }
        FCollisionQueryParams Params(SCENE_QUERY_STAT(SovMeleeAimLOS),false,ActionAvatar.Get()); SovSelenePayload::IgnoreSource(Params,ActionAvatar.Get());
        FHitResult Hit;
        if (GetWorld()->LineTraceSingleByChannel(Hit,Source,Candidate->GetActorLocation(),ECC_Visibility,Params)
            &&SovSelenePayload::ResolveTarget(Hit.GetActor())!=Candidate) { continue; }
        Best=Candidate; BestAngle=FMath::Abs(Yaw); DesiredYaw=Yaw;
    }
    if (Best)
    {
        FRotator Rotation=ActionAvatar->GetActorRotation();
        Rotation.Yaw+=static_cast<float>(SovMelee::AimCorrection(DesiredYaw,Node.MaximumAimCorrection,Strength));
        ActionAvatar->SetActorRotation(Rotation);
    }
}
void USovGameplayAbility_Melee::StartAttackWindow()
{
    if (!NodeGeometryValid()||!CanDispatchNativeAttack()) { FinishMelee(); return; }
    FGuid ExpectedAttack, CurrentAttack;
    if (!GetSovAttackIdentity(ActionAvatar.Get(),ExpectedAttack)) { FinishMelee(); return; }
    ApplyAimCorrection();
    if (!GetSovAttackIdentity(GetAvatarActorFromActorInfo(),CurrentAttack)||CurrentAttack!=ExpectedAttack) { return; }
    if (!NodeGeometryValid()) { FinishMelee(); return; }
    const auto& Node=AttackDefinition->Nodes[NodeIndex];
    AttackReceipt=USovEchoAttackReceipt::CreateForActiveAbility(this);
    if (!AttackReceipt) { FinishMelee(); return; }
    if (Node.Montage) { ActionASC->PlayMontage(this,CurrentActivationInfo,Node.Montage,1.f,Node.Section); }
    if (!GetSovAttackIdentity(GetAvatarActorFromActorInfo(),CurrentAttack)||CurrentAttack!=ExpectedAttack) { return; }
    if (!NodeGeometryValid()) { FinishMelee(); return; }
    FGameplayTagContainer Inputs;
    if (Node.NextNode!=INDEX_NONE) { Inputs.AddTag(Node.FollowUpInput); }
    if (Node.DefensiveInput.IsValid()) { Inputs.AddTag(Node.DefensiveInput); }
    if (auto* ASC=Cast<UNarrativeAbilitySystemComponent>(ActionASC.Get()))
    { InputWindow=ASC->RegisterCombatInputWindow(this,Inputs,FMath::Max(Node.BranchOpen-Node.HitConfirmAdvance,Node.Startup),Node.BranchClose); }
    SweepTask=USovAbilityTask_MeleeSweep::SweepMeleeSockets(this,ActionMesh.Get(),Node);
    SweepTask->OnContact.AddDynamic(this,&ThisClass::OnContact); SweepTask->OnEnvironmentContact.AddDynamic(this,&ThisClass::OnEnvironment);
    SweepTask->OnStep.AddDynamic(this,&ThisClass::OnStep); SweepTask->OnFinished.AddDynamic(this,&ThisClass::OnNodeFinished);
    SweepTask->OnInvalidated.AddDynamic(this,&ThisClass::OnTaskInvalidated); SweepTask->ReadyForActivation();
}
void USovGameplayAbility_Melee::OnContact(const FHitResult& Hit,AActor* Target)
{
    if (!NodeGeometryValid()||!CanDispatchNativeAttack()||!AttackReceipt||!IsValid(Target)) { FinishMelee(); return; }
    FGuid ExpectedAttack;
    if (!GetSovAttackIdentity(ActionAvatar.Get(),ExpectedAttack)) { FinishMelee(); return; }
    auto* Source=Cast<UNarrativeAbilitySystemComponent>(ActionASC.Get()); auto* TargetASC=UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
    if (!Source||!TargetASC||TargetASC->GetAvatarActor()!=Target||TargetASC==Source
        ||TargetASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute())<=0.f
        ||TargetASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_IsDead)
        ||TargetASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Fatal)) { return; }
    const auto& Node=AttackDefinition->Nodes[NodeIndex]; const auto& T=FSovGameplayTags::Get();
    FGameplayEffectContextHandle Context=Source->MakeEffectContext(); Context.AddInstigator(ActionAvatar.Get(),ActionAvatar.Get()); Context.AddSourceObject(AttackReceipt); Context.AddHitResult(Hit,true);
    FGameplayEffectSpec Spec(GetDefault<USovGameplayEffect_MeleeDamage>(),Context,GetAbilityLevel());
    if (Node.DamageChannels.IsEmpty()) { Spec.AddDynamicAssetTag(T.Damage_Channel_Edge); }
    else { for (const auto& Tag:Node.DamageChannels) { Spec.AddDynamicAssetTag(Tag); } }
    for (const auto& Tag:Node.AttackClassifications) { Spec.AddDynamicAssetTag(Tag); }
    Spec.SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Damage,Node.Damage);
    Spec.SetSetByCallerMagnitude(T.SetByCaller_Damage_AbilityScalar,ChargeScalar);
    Spec.SetSetByCallerMagnitude(T.SetByCaller_Damage_ShieldCoefficient,Node.ShieldCoefficient);
    Spec.SetSetByCallerMagnitude(T.SetByCaller_Damage_HealthCoefficient,Node.HealthCoefficient);
    Spec.SetSetByCallerMagnitude(T.SetByCaller_Damage_PoiseDamage,Node.PoiseDamage*ChargeScalar);
    TStrongObjectPtr<USovNativeDamageReceipt> Receipt(NewObject<USovNativeDamageReceipt>()); Receipt->ExpectedContext=Context.Get(); Receipt->ExpectedTarget=Target;
    Source->OnDamageResolvedAsSource.AddDynamic(Receipt.Get(),&USovNativeDamageReceipt::ReceiveResult);
    Source->ApplyGameplayEffectSpecToTarget(Spec,TargetASC);
    if (IsValid(Source)) { Source->OnDamageResolvedAsSource.RemoveDynamic(Receipt.Get(),&USovNativeDamageReceipt::ReceiveResult); }
    FGuid CurrentAttack;
    if (!ContextValid()||!GetSovAttackIdentity(ActionAvatar.Get(),CurrentAttack)||CurrentAttack!=ExpectedAttack) { return; }
    bHitConfirmed|=Receipt->bAppliedDamage; OnMeleeImpact(Hit,Target,Receipt->bAppliedDamage);
}
void USovGameplayAbility_Melee::OnEnvironment(const FHitResult& Hit,AActor* Target)
{ if (ContextValid()) { OnMeleeEnvironmentContact(Hit); } }
void USovGameplayAbility_Melee::OnStep(float Elapsed)
{
    if (!NodeGeometryValid()) { FinishMelee(); return; }
    const auto Node=AttackDefinition->Nodes[NodeIndex];
    if (Elapsed<Node.BranchOpen-(bHitConfirmed?Node.HitConfirmAdvance:0.f)) { return; }
    auto* ASC=Cast<UNarrativeAbilitySystemComponent>(ActionASC.Get()); FGameplayTag Input; bool bHeld=false;
    if (!ASC||!ASC->ConsumeCombatInputWindow(this,InputWindow,Input,bHeld)) { return; }
    if (Input==Node.FollowUpInput&&SovMelee::ValidFollowUp(NodeIndex,Node.NextNode,AttackDefinition->Nodes.Num()))
    {
        // A buffered press retains its release edge. Entering a charge node cannot manufacture
        // a held input after the player already let go during the preceding recovery.
        if (BeginNode(Node.NextNode) && bCharging && !bHeld) { ReleaseCharge(); }
        return;
    }
    if (Input==Node.DefensiveInput&&Node.DefensiveAbility)
    {
        const FGameplayAbilitySpec* Spec=ASC->FindAbilitySpecFromClass(Node.DefensiveAbility);
        const auto* Evade=Spec?Cast<USovGameplayAbility_Evade>(Spec->Ability):nullptr;
        if (!Spec||!Evade||!Evade->CanActivateAfterCombatCancel(Spec->Handle,CurrentActorInfo,this,Node.DefensiveCancelCost)) { return; }
        const auto Handle=Spec->Handle; TWeakObjectPtr<AActor> Avatar=ActionAvatar;
        if (!TryCommitDefensiveCancel(Node.DefensiveCancelCost)||!ContextValid()) { return; }
        FinishMelee();
        if (IsValid(ASC)&&Avatar.IsValid()&&ASC->GetAvatarActor()==Avatar.Get()) { ASC->TryActivateAbility(Handle,false); }
    }
}
void USovGameplayAbility_Melee::OnNodeFinished() { FinishMelee(); }
void USovGameplayAbility_Melee::OnTaskInvalidated() { FinishMelee(); }
void USovGameplayAbility_Melee::FinishMelee()
{ if (IsActive()) { EndAbility(CurrentSpecHandle,CurrentActorInfo,CurrentActivationInfo,true,false); } }
void USovGameplayAbility_Melee::EndAbility(const FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* Info,
    const FGameplayAbilityActivationInfo Activation,bool bReplicate,bool bCancelled)
{
    if (!IsEndAbilityValid(Handle,Info)) { return; }
    if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(ChargeTimer); }
    auto* ASC=Cast<UNarrativeAbilitySystemComponent>(ActionASC.Get());
    if (ASC) { ASC->ClearCombatInputWindow(this,InputWindow); }
    InputWindow.Invalidate(); bCharging=false;
    if (SweepTask) { SweepTask->EndTask(); SweepTask=nullptr; }
    ActionAvatar.Reset(); ActionASC.Reset(); ActionMesh.Reset(); ActionWeapon.Reset(); AttackReceipt=nullptr; NodeIndex=INDEX_NONE;
    if (IsValid(ASC)&&ASC->GetAnimatingAbility()==this) { ASC->CurrentMontageStop(.1f); }
    Super::EndAbility(Handle,Info,Activation,bReplicate,bCancelled);
}
