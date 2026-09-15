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
#include "Sovereign/SovEnvironmentDamage.h"
#include "Targeting/SovTargetingComponent.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "UnrealFramework/NarrativeGameUserSettings.h"
#include "UObject/StrongObjectPtr.h"
#include "Weapons/WeaponVisual.h"
namespace
{
    const FGameplayTagContainer& MeleeTransactionInterruptions()
    {
        static const FGameplayTagContainer Tags=[]()
        {
            const auto& N=FNarrativeGameplayTags::Get(); const auto& S=FSovGameplayTags::Get();
            FGameplayTagContainer Result;
            for (FGameplayTag Tag : {N.State_IsDead,N.State_Interacting,N.State_SequencerControlled,
                N.State_Movement_Ragdoll,N.State_Weapon_Equipping,S.State_Fatal,S.State_Poise_Broken,
                S.State_Guard_Broken,S.State_Status_Frozen,S.State_Weapon_VerityAbsent}) { Result.AddTag(Tag); }
            return Result;
        }();
        return Tags;
    }
}
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
    ActivationBlockedTags.AppendTags(MeleeTransactionInterruptions());
}
bool USovGameplayAbility_Melee::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* Info,
    const FGameplayTagContainer* SourceTags,const FGameplayTagContainer* TargetTags,FGameplayTagContainer* Relevant) const
{
    FString Error;
    const auto* ASC=Info?Info->AbilitySystemComponent.Get():nullptr;
    const float Health=ASC?ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()):0.f;
    return !bEndingMelee&&Info&&Info->IsNetAuthority()&&Info->AvatarActor.IsValid()&&Info->AbilitySystemComponent.IsValid()
        &&FMath::IsFinite(Health)&&Health>0.f
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
    const auto* NarrativeASC=Cast<UNarrativeAbilitySystemComponent>(ActionASC.Get());
    return !bMeleeEndPending&&!bEndingMelee&&IsActive()&&ActionAvatar.IsValid()&&ActionASC.IsValid()&&!ActionAvatar->IsActorBeingDestroyed()
        &&CurrentActorInfo&&CurrentActorInfo->IsNetAuthority()&&CurrentActorInfo->AvatarActor.Get()==ActionAvatar.Get()
        &&CurrentActorInfo->AbilitySystemComponent.Get()==ActionASC.Get()&&ActionASC->GetAvatarActor()==ActionAvatar.Get()
        &&(!NarrativeASC||NarrativeASC->GetCombatActorInfoEpoch()==ActionActorInfoEpoch)
        &&ActionAttributes.IsValid()&&ActionASC->GetSet<UNarrativeAttributeSetBase>()==ActionAttributes.Get()
        &&ActionAttributes->GetCombatLifeEpoch()==ActionLifeEpoch
        &&UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(ActionAvatar.Get())==ActionASC.Get()
        &&ActionASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute())>0.f
        &&!ActionASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_IsDead)
        &&!ActionASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Fatal)
        &&!ActionASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Weapon_Equipping)
        &&!ActionASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_SequencerControlled)
        &&!ActionASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Weapon_VerityAbsent)
        &&!ActionASC->HasAnyMatchingGameplayTags(MeleeTransactionInterruptions())
        &&ActionASC->GetGameplayTagCount(FNarrativeGameplayTags::Get().State_Busy)<=1
        &&AttackDefinition&&AttackDefinition->Nodes.IsValidIndex(NodeIndex)
        &&((!bStartedUnarmed&&ActionWeapon.IsValid()&&GetOwnerEquippedWeapon(IsMainhand())==ActionWeapon.Get())
            ||(bStartedUnarmed&&bAllowUnarmed&&!GetOwnerEquippedWeapon(IsMainhand())));
}
void USovGameplayAbility_Melee::BindInterruptions()
{
    UnbindInterruptions();
    if (!ActionASC.IsValid()) { return; }
    FGameplayTagContainer Tags=MeleeTransactionInterruptions(); Tags.AddTag(FNarrativeGameplayTags::Get().State_Busy);
    for (FGameplayTag Tag:Tags)
    {
        InterruptionHandles.Add(Tag,ActionASC->RegisterGameplayTagEvent(Tag,EGameplayTagEventType::AnyCountChange)
            .AddUObject(this,&ThisClass::HandleInterruption));
    }
    if (!ContextValid()) { FinishMelee(); }
}
void USovGameplayAbility_Melee::UnbindInterruptions()
{
    if (ActionASC.IsValid())
    {
        for (const auto& Entry:InterruptionHandles)
        { ActionASC->RegisterGameplayTagEvent(Entry.Key,EGameplayTagEventType::AnyCountChange).Remove(Entry.Value); }
    }
    InterruptionHandles.Reset();
}
void USovGameplayAbility_Melee::HandleInterruption(FGameplayTag Tag,int32 Count)
{
    if (Count>(Tag==FNarrativeGameplayTags::Get().State_Busy?1:0)) { FinishMelee(); }
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
    const uint64 Epoch=++MeleeActivationEpoch;
    TStrongObjectPtr<USovGameplayAbility_Melee> ActionLifetime(this);
    bMeleeEndPending=false;
    const TWeakObjectPtr<AActor> Avatar=Info?Info->AvatarActor.Get():nullptr;
    const TWeakObjectPtr<UAbilitySystemComponent> ASC=Info?Info->AbilitySystemComponent.Get():nullptr;
    const auto* NarrativeASC=Cast<UNarrativeAbilitySystemComponent>(ASC.Get());
    const uint64 ActorInfoEpoch=NarrativeASC?NarrativeASC->GetCombatActorInfoEpoch():0;
    const TWeakObjectPtr<const UNarrativeAttributeSetBase> Attributes=ASC.IsValid()?ASC->GetSet<UNarrativeAttributeSetBase>():nullptr;
    const uint64 LifeEpoch=Attributes.IsValid()?Attributes->GetCombatLifeEpoch():0;
    const auto ContinueActivation=[this,Epoch,Handle,Avatar,ASC,ActorInfoEpoch,Attributes,LifeEpoch]()
    {
        const auto* NativeASC=Cast<UNarrativeAbilitySystemComponent>(ASC.Get());
        if (MeleeActivationEpoch==Epoch&&!bMeleeEndPending&&!bEndingMelee&&IsActive()
            &&CurrentActorInfo&&CurrentSpecHandle==Handle&&CurrentActorInfo->IsNetAuthority()
            &&Avatar.IsValid()&&!Avatar->IsActorBeingDestroyed()&&ASC.IsValid()
            &&CurrentActorInfo->AvatarActor==Avatar&&CurrentActorInfo->AbilitySystemComponent==ASC
            &&(!NativeASC||NativeASC->GetCombatActorInfoEpoch()==ActorInfoEpoch)
            &&Attributes.IsValid()&&ASC->GetSet<UNarrativeAttributeSetBase>()==Attributes.Get()
            &&Attributes->GetCombatLifeEpoch()==LifeEpoch
            &&ASC->GetAvatarActor()==Avatar.Get()) { return true; }
        if (MeleeActivationEpoch==Epoch&&IsActive()) { FinishMelee(); }
        return false;
    };
    Super::ActivateAbility(Handle,Info,Activation,Event);
    if (!ContinueActivation()) { return; }
    ActionASC=ASC; ActionAvatar=Avatar; ActionActorInfoEpoch=ActorInfoEpoch;
    ActionAttributes=Attributes; ActionLifeEpoch=LifeEpoch;
    const TWeakObjectPtr<UWeaponItem> Weapon=GetOwnerEquippedWeapon(IsMainhand());
    if (!ContinueActivation()) { return; }
    ActionWeapon=Weapon; bStartedUnarmed=!Weapon.IsValid(); NodeIndex=0;
    USkeletalMeshComponent* Mesh=ResolveMeleeTraceMesh();
    if (!ContinueActivation()) { return; }
    ActionMesh=Mesh;
    FGuid ExpectedAttack;
    if (!GetSovAttackIdentity(ActionAvatar.Get(),ExpectedAttack)) { FinishMelee(); return; }
    const bool bGeometryValid=NodeGeometryValid();
    if (!ContinueActivation()) { return; }
    const bool bCommitted=bGeometryValid&&CommitAbility(Handle,Info,Activation);
    if (!ContinueActivation()) { return; }
    FGuid CurrentAttack;
    if (!GetSovAttackIdentity(GetAvatarActorFromActorInfo(),CurrentAttack)||CurrentAttack!=ExpectedAttack) { return; }
    if (!bCommitted||!ContextValid()) { FinishMelee(); return; }
    BindInterruptions();
    if (!ContinueActivation()||!ContextValid()) { return; }
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
    bMeleeCharging=Node.bCharged;
    if (bMeleeCharging)
    {
        ChargeStarted=GetWorld()->GetTimeSeconds();
        const uint64 Epoch=MeleeActivationEpoch;
        GetWorld()->GetTimerManager().SetTimer(ChargeTimer,FTimerDelegate::CreateWeakLambda(this,[this,Epoch,ExpectedAttack]()
        {
            FGuid CurrentAttack;
            if (MeleeActivationEpoch==Epoch&&ContextValid()&&GetSovAttackIdentity(ActionAvatar.Get(),CurrentAttack)
                &&CurrentAttack==ExpectedAttack) { ReleaseCharge(); }
        }),Node.MaximumChargeSeconds,false);
    }
    else { StartAttackWindow(); }
    FGuid CurrentAttack;
    if (!ContextValid()||!GetSovAttackIdentity(ActionAvatar.Get(),CurrentAttack)||CurrentAttack!=ExpectedAttack) { return false; }
    OnMeleeNodeStarted(NodeIndex,bMeleeCharging);
    return ContextValid()&&GetSovAttackIdentity(ActionAvatar.Get(),CurrentAttack)&&CurrentAttack==ExpectedAttack;
}
void USovGameplayAbility_Melee::InputReleased(const FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* Info,const FGameplayAbilityActivationInfo Activation)
{
    const uint64 Epoch=MeleeActivationEpoch;
    Super::InputReleased(Handle,Info,Activation);
    if (MeleeActivationEpoch==Epoch&&ContextValid()&&bMeleeCharging) { ReleaseCharge(); }
}
void USovGameplayAbility_Melee::ReleaseCharge()
{
    if (!bMeleeCharging) { return; } bMeleeCharging=false; GetWorld()->GetTimerManager().ClearTimer(ChargeTimer);
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
    const auto* Settings=UNarrativeGameUserSettings::GetSovPlayerSettings(ActionAvatar.Get());
    const float Strength=Settings?Settings->GetMeleeAimAssistStrength():0.f;
    if (Strength<=0.f||!ContextValid()) { return; }
    const uint64 Epoch=MeleeActivationEpoch;
    const TWeakObjectPtr<AActor> SourceAvatar=ActionAvatar;
    FGuid ExpectedAttack;
    if (!GetSovAttackIdentity(SourceAvatar.Get(),ExpectedAttack)) { return; }
    const auto StillOwnsAttack=[this,Epoch,SourceAvatar,ExpectedAttack]()
    {
        FGuid CurrentAttack;
        return MeleeActivationEpoch==Epoch&&ContextValid()&&ActionAvatar==SourceAvatar
            &&GetSovAttackIdentity(SourceAvatar.Get(),CurrentAttack)&&CurrentAttack==ExpectedAttack;
    };
    // Team policy is virtual and can reenter GAS. Do not retain node references
    // or mutate a replacement avatar after that callback.
    const auto Node=AttackDefinition->Nodes[NodeIndex]; const FVector Source=SourceAvatar->GetActorLocation();
    TWeakObjectPtr<AActor> Best; float BestAngle=Node.MaximumAimCorrection;
    TArray<TWeakObjectPtr<AActor>> Candidates;
    if (const auto* Targeting=ActionAvatar->FindComponentByClass<USovTargetingComponent>())
    { if (AActor* Locked=Targeting->GetLockedTarget()) { Candidates.Add(Locked); } }
    TArray<FOverlapResult> Overlaps; FCollisionObjectQueryParams Objects; Objects.AddObjectTypesToQuery(ECC_Pawn);
    GetWorld()->OverlapMultiByObjectType(Overlaps,Source,FQuat::Identity,Objects,FCollisionShape::MakeSphere(250.f),FCollisionQueryParams(SCENE_QUERY_STAT(SovMeleeAim),false,ActionAvatar.Get()));
    for (const auto& Hit:Overlaps) { if (Hit.GetActor()) { Candidates.AddUnique(Hit.GetActor()); } }
    float DesiredYaw=0.f;
    for (const TWeakObjectPtr<AActor>& WeakCandidate:Candidates)
    {
        if (!StillOwnsAttack()) { return; }
        if (!WeakCandidate.IsValid()||WeakCandidate->IsActorBeingDestroyed()) { continue; }
        TStrongObjectPtr<AActor> CandidateLifetime(WeakCandidate.Get()); AActor* Candidate=CandidateLifetime.Get();
        const auto* ASC=UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Candidate);
        if (!ASC||ASC->GetAvatarActor()!=Candidate
            ||ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute())<=0.f) { continue; }
        const ETeamAttitude::Type Attitude=UArsenalStatics::GetAttitude(SourceAvatar.Get(),Candidate);
        if (!StillOwnsAttack()) { return; }
        if (!IsValid(Candidate)||Candidate->IsActorBeingDestroyed()||Attitude!=ETeamAttitude::Hostile
            ||FVector::DistSquared(Source,Candidate->GetActorLocation())>FMath::Square(250.f)) { continue; }
        const float Yaw=FMath::FindDeltaAngleDegrees(ActionAvatar->GetActorRotation().Yaw,(Candidate->GetActorLocation()-Source).Rotation().Yaw);
        if (FMath::Abs(Yaw)>BestAngle) { continue; }
        FCollisionQueryParams Params(SCENE_QUERY_STAT(SovMeleeAimLOS),false,ActionAvatar.Get()); SovSelenePayload::IgnoreSource(Params,ActionAvatar.Get());
        FHitResult Hit;
        if (GetWorld()->LineTraceSingleByChannel(Hit,Source,Candidate->GetActorLocation(),ECC_Visibility,Params)
            &&SovSelenePayload::ResolveTarget(Hit.GetActor())!=Candidate) { continue; }
        Best=Candidate; BestAngle=FMath::Abs(Yaw); DesiredYaw=Yaw;
    }
    if (Best.IsValid()&&!Best->IsActorBeingDestroyed()&&StillOwnsAttack())
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
{
    if (!ContextValid()) { return; }
    if (IsValid(Hit.GetActor()) && Hit.GetActor()->Implements<USovEnvironmentDamageable>()) { PendingEnvironmentDamage.Add(Hit); }
    OnMeleeEnvironmentContact(Hit);
}
void USovGameplayAbility_Melee::OnStep(float Elapsed)
{
    if (!NodeGeometryValid()) { FinishMelee(); return; }
    const auto Node=AttackDefinition->Nodes[NodeIndex];
    if (!PendingEnvironmentDamage.IsEmpty())
    {
        const TArray<FHitResult> Contacts=MoveTemp(PendingEnvironmentDamage); PendingEnvironmentDamage.Reset();
        for (const FHitResult& Contact:Contacts)
        { if (CanDispatchNativeAttack()) { SovEnvironmentDamage::ApplyPoint(ActionAvatar.Get(),Contact,Node.Damage*ChargeScalar); } }
        if (!NodeGeometryValid()) { FinishMelee(); return; }
    }
    if (Elapsed<Node.BranchOpen-(bHitConfirmed?Node.HitConfirmAdvance:0.f)) { return; }
    auto* ASC=Cast<UNarrativeAbilitySystemComponent>(ActionASC.Get()); FGameplayTag Input; bool bHeld=false;
    if (!ASC||!ASC->ConsumeCombatInputWindow(this,InputWindow,Input,bHeld)) { return; }
    if (Input==Node.FollowUpInput&&SovMelee::ValidFollowUp(NodeIndex,Node.NextNode,AttackDefinition->Nodes.Num()))
    {
        // A buffered press retains its release edge. Entering a charge node cannot manufacture
        // a held input after the player already let go during the preceding recovery.
        if (BeginNode(Node.NextNode) && bMeleeCharging && !bHeld) { ReleaseCharge(); }
        return;
    }
    if (Input==Node.DefensiveInput&&Node.DefensiveAbility)
    {
        const FGameplayAbilitySpec* Spec=ASC->FindAbilitySpecFromClass(Node.DefensiveAbility);
        const auto* Evade=Spec?Cast<USovGameplayAbility_Evade>(Spec->Ability):nullptr;
        if (!Spec||!Evade||!Evade->CanActivateAfterCombatCancel(Spec->Handle,CurrentActorInfo,this,Node.DefensiveCancelCost)) { return; }
        const auto Handle=Spec->Handle; TWeakObjectPtr<AActor> Avatar=ActionAvatar;
        const uint64 Epoch=MeleeActivationEpoch;
        if (!TryCommitDefensiveCancel(Node.DefensiveCancelCost)||MeleeActivationEpoch!=Epoch||!ContextValid()) { return; }
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
    if (bEndingMelee||!IsEndAbilityValid(Handle,Info)) { return; }
    ++MeleeActivationEpoch;
    bMeleeEndPending=true;
    PendingEnvironmentDamage.Reset();
    if (ScopeLockCount>0) { Super::EndAbility(Handle,Info,Activation,bReplicate,bCancelled); return; }
    TStrongObjectPtr<USovGameplayAbility_Melee> ActionLifetime(this);
    TGuardValue<bool> Ending(bEndingMelee,true);
    UnbindInterruptions();
    if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(ChargeTimer); }
    auto* ASC=Cast<UNarrativeAbilitySystemComponent>(ActionASC.Get());
    if (ASC) { ASC->ClearCombatInputWindow(this,InputWindow); }
    InputWindow.Invalidate(); bMeleeCharging=false;
    if (SweepTask) { SweepTask->EndTask(); SweepTask=nullptr; }
    ActionAvatar.Reset(); ActionASC.Reset(); ActionAttributes.Reset(); ActionMesh.Reset(); ActionWeapon.Reset(); AttackReceipt=nullptr; NodeIndex=INDEX_NONE;
    if (IsValid(ASC)&&ASC->GetAnimatingAbility()==this) { ASC->CurrentMontageStop(.1f); }
    Super::EndAbility(Handle,Info,Activation,bReplicate,bCancelled);
}
