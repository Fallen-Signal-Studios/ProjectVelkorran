// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Components/SovAurelionThermalFractureComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Campaign/SovCampaignDefinition.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Campaign/SovEncounterDirector.h"
#include "Combat/SovSelenePayload.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Companions/SovCompanionComponent.h"
#include "Companions/SovConvergenceCompanionState.h"
#include "Companions/SovProtagonistCompanionCharacter.h"
#include "Components/SovPoiseComponent.h"
#include "Components/SovWeakPointComponent.h"
#include "Effects/SovGameplayEffect_CinderGrenade.h"
#include "Effects/SovGameplayEffect_SeleneControl.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/SovPlayerController.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "NarrativeGameplayTags.h"
#include "NativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "UnrealFramework/NarrativeCharacter.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AurelionFrostSetup, "Sov.Ability.Context.AurelionFrostSetup");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AurelionHeatConfirm, "Sov.Ability.Context.AurelionHeatConfirm");

namespace
{
TArray<FGameplayTag> HeroInterruptTags()
{
    const auto& N = FNarrativeGameplayTags::Get(); const auto& S = FSovGameplayTags::Get();
    return {N.State_IsDead, N.State_SequencerControlled, N.State_Movement_Ragdoll, N.State_Movement_Lock,
        N.State_Interacting, N.State_Busy, S.State_Fatal, S.State_Poise_Broken};
}
bool HeroIsInterrupted(const UNarrativeAbilitySystemComponent* ASC)
{
    if (!IsValid(ASC)) { return true; }
    for (const auto Tag : HeroInterruptTags()) { if (ASC->HasMatchingGameplayTag(Tag)) { return true; } }
    return false;
}
}

USovAurelionThermalFractureComponent::USovAurelionThermalFractureComponent()
{ PrimaryComponentTick.bCanEverTick = false; }

void USovAurelionThermalFractureComponent::BeginPlay()
{
    Super::BeginPlay();
    if (auto* Character = Cast<ANarrativeCharacter>(GetOwner()))
    { Character->OnASCInitialized.AddUniqueDynamic(this, &ThisClass::HandleOwnerASCInitialized); }
    InitializeBindings();
}
void USovAurelionThermalFractureComponent::HandleOwnerASCInitialized() { InitializeBindings(); }
bool USovAurelionThermalFractureComponent::InitializeBindings()
{
    if (bEnding || !IsValid(GetOwner()) || !GetOwner()->HasAuthority() || !GetWorld() || GetWorld()->GetNetMode() != NM_Standalone) { return false; }
    auto* ASC = Cast<UNarrativeAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()));
    ASovEncounterDirector* Director = nullptr;
    for (TActorIterator<ASovEncounterDirector> It(GetWorld()); It; ++It)
    {
        if (!It->IsActorBeingDestroyed() && !It->FindParticipantId(GetOwner()).IsNone())
        { if (Director) { LastError = TEXT("The Thermal Fracture elite belongs to multiple directors."); return false; } Director = *It; }
    }
    if (!IsValid(ASC) || ASC->GetAvatarActor() != GetOwner() || !Director || (EncounterDirector && EncounterDirector != Director)
        || Director->GetCampaignProofType() != ESovEncounterProofType::AurelionThermalFracture)
    { LastError = TEXT("Thermal Fracture needs its initialized elite ASC and unique encounter registration."); return false; }
    // Restored pawns do not retain the level instance's optional authoring pointer.
    // Publish only the uniquely validated owner so stable request controls can resolve it.
    EncounterDirector = Director;
    if (BoundASC == ASC && BoundDirector == Director) { return true; }
    Unbind(); Retire(); BoundASC = ASC; BoundDirector = Director;
    ControlDelegate = ASC->OnActiveGameplayEffectAddedDelegateToSelf.AddUObject(this, &ThisClass::HandleControlApplied);
    ASC->OnDamageResolvedAsTarget.AddUniqueDynamic(this, &ThisClass::HandleDamage);
    Director->OnEncounterStateChanged.AddUniqueDynamic(this, &ThisClass::HandleEncounterState);
    return true;
}
bool USovAurelionThermalFractureComponent::CaptureContext(FContext& Out) const
{
    const auto& Tags = FSovGameplayTags::Get();
    auto* Director = BoundDirector.Get();
    auto* Player = Director ? Director->GetEncounterPlayer() : nullptr;
    auto* PC = IsValid(Player) ? Cast<ASovPlayerController>(Player->GetController()) : nullptr;
    auto* CompanionState = PC ? PC->GetConvergenceCompanionState() : nullptr;
    auto* Selene = CompanionState ? CompanionState->GetActiveCompanion() : nullptr;
    auto* PlayerASC = IsValid(Player) ? Player->GetNarrativeAbilitySystemComponent() : nullptr;
    auto* SeleneASC = IsValid(Selene) ? Selene->GetNarrativeAbilitySystemComponent() : nullptr;
    if (bEnding || !IsValid(GetOwner()) || !GetOwner()->HasAuthority() || !BoundASC.IsValid() || !Director || Director->IsActorBeingDestroyed()
        || Director->GetEncounterState() != ESovEncounterState::Active || !Director->GetAttemptId().IsValid()
        || Director->FindParticipantId(GetOwner()).IsNone() || !IsValid(PC) || !IsValid(PlayerASC) || !IsValid(SeleneASC)
        || !PC->GetCampaignState() || !PC->GetCampaignState()->GetActiveMission() || !PC->GetCampaignState()->IsStateValid()
        || PC->GetCampaignState()->GetActiveProtagonist() != Tags.Character_Player_Tarrik
        || Selene->GetCompanionIdentity() != Tags.Character_Player_Selene || !Selene->GetCompanionComponent()
        || Selene->GetCompanionComponent()->CompanionId != TEXT("Selene") || Selene->GetCompanionComponent()->IsDisabled()
        || Selene->GetCompanionComponent()->GetCurrentLeader() != Player) { return false; }
    Out.Director = Director; Out.Player = Player; Out.Controller = PC; Out.Selene = Selene;
    Out.PlayerASC = PlayerASC; Out.SeleneASC = SeleneASC; Out.TargetASC = BoundASC;
    Out.Campaign = PC->GetCampaignState(); Out.Mission = Out.Campaign->GetActiveMission();
    Out.AttemptId = Director->GetAttemptId(); Out.EncounterId = Director->EncounterId; Out.Generation = Director->GetLifecycleGeneration();
    Out.TransitionEpoch = PC->GetCampaignTransitionEpoch(); Out.PlayerReadyEpoch = PlayerASC->GetCharacterReadyEpoch();
    Out.SeleneReadyEpoch = SeleneASC->GetCharacterReadyEpoch(); Out.PlayerActorInfoEpoch = PlayerASC->GetCombatActorInfoEpoch();
    Out.SeleneActorInfoEpoch = SeleneASC->GetCombatActorInfoEpoch(); Out.TargetActorInfoEpoch = BoundASC->GetCombatActorInfoEpoch();
    return IsContextCurrent(Out, true);
}
bool USovAurelionThermalFractureComponent::HasLineOfSight(AActor* Source, AActor* Target) const
{
    if (!IsValid(Source) || !IsValid(Target) || !GetWorld()) { return false; }
    FVector Origin; FRotator Rotation; Source->GetActorEyesViewPoint(Origin, Rotation);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(AurelionThermalSight), false, Source);
    SovSelenePayload::IgnoreSource(Params, Source);
    FHitResult Hit;
    return !GetWorld()->LineTraceSingleByChannel(Hit, Origin, Target->GetActorLocation(), ECC_Visibility, Params)
        || SovSelenePayload::ResolveTarget(Hit.GetActor()) == Target;
}
bool USovAurelionThermalFractureComponent::ValidateCleanAnchor(const FContext& C, FString& Error) const
{
    if (!IsContextCurrent(C, true) || !IsValid(FrostAnchor) || FrostAnchor->IsActorBeingDestroyed()
        || FrostAnchor->GetWorld() != GetWorld() || FrostAnchorId.IsNone() || !FrostAnchor->ActorHasTag(FrostAnchorId)
        || !FMath::IsFinite(FrostAnchorReach) || FrostAnchorReach < 50.f || FrostAnchorReach > 300.f
        || !FMath::IsFinite(FrostSetupRange) || FrostSetupRange < 100.f || FrostSetupRange > 2500.f
        || !FMath::IsFinite(FractureWindowSeconds) || FractureWindowSeconds < .25f || FractureWindowSeconds > 10.f)
    { Error = TEXT("A ready Selene companion and a uniquely named clean frost anchor are required."); return false; }
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        if (*It != FrostAnchor && !It->IsActorBeingDestroyed() && It->ActorHasTag(FrostAnchorId))
        { Error = TEXT("The clean frost anchor tag is duplicated."); return false; }
    }
    if (FVector::DistSquared(C.Selene->GetActorLocation(), FrostAnchor->GetActorLocation()) > FMath::Square(FrostAnchorReach)
        || FVector::DistSquared(C.Selene->GetActorLocation(), GetOwner()->GetActorLocation()) > FMath::Square(FrostSetupRange)
        || FVector::DistSquared(C.Player->GetActorLocation(), FrostAnchor->GetActorLocation()) > FMath::Square(FrostSetupRange)
        || !HasLineOfSight(C.Selene.Get(), GetOwner()))
    { Error = TEXT("Selene must occupy the clean frost mark and see the nearby elite while Tarrik remains in support range."); return false; }
    FCollisionQueryParams Params(SCENE_QUERY_STAT(AurelionFrostGround), false, C.Selene.Get());
    SovSelenePayload::IgnoreSource(Params, C.Selene.Get()); SovSelenePayload::IgnoreSource(Params, C.Player.Get());
    SovSelenePayload::IgnoreSource(Params, GetOwner()); Params.AddIgnoredActor(FrostAnchor);
    const FVector Location = C.Selene->GetActorLocation(); FHitResult Ground;
    if (!GetWorld()->LineTraceSingleByChannel(Ground, Location + FVector(0,0,20), Location - FVector(0,0,250), ECC_Visibility, Params)
        || !Ground.bBlockingHit || Ground.ImpactNormal.Z < .7f)
    { Error = TEXT("The clean frost mark requires a grounded, walkable position for Selene."); return false; }
    return IsContextCurrent(C, true);
}
bool USovAurelionThermalFractureComponent::RequestFrostSetup(ASovPlayerCharacterBase* Tarrik, FString& Error)
{
    Error.Reset(); FContext Candidate;
    if (bFrostSetupExecuting || bConfirmExecuting || bPayoffExecuting || bPayoffPending || Receipt.IsComplete()
        || GetFractureWindowRemainingSeconds() > 0.f || !GetWorld() || GetWorld()->GetTimeSeconds() < NextFrostSetupAt
        || !CaptureContext(Candidate) || Candidate.Player.Get() != Tarrik)
    { Error = TEXT("Frost setup requires the current Tarrik attempt, a closed window and no pending fracture."); LastError = Error; return false; }
    if (!ValidateCleanAnchor(Candidate, Error)) { LastError = Error; return false; }
    TGuardValue<bool> SettingUp(bFrostSetupExecuting, true);
    NextFrostSetupAt = GetWorld()->GetTimeSeconds() + 1.f;
    FSovSelenePayloadContext Payload; Payload.SourceASC = Candidate.SeleneASC; Payload.SourceAvatar = Candidate.Selene;
    Payload.SourceObject = this; Payload.AbilityTag = TAG_AurelionFrostSetup;
    // Control owns actual freeze/chill, movement restriction, immunity and cancellation rules.
    // This local interaction carries a context tag, never the identity or cost of an Echo spender.
    SovSelenePayload::Control(Payload, GetOwner(), FractureWindowSeconds, 0.f, true);
    const bool bOpened = IsContextCurrent(Candidate, true) && GetFractureWindowRemainingSeconds() > 0.f;
    if (!bOpened) { Error = TEXT("The elite did not accept a fresh frost control effect. Reposition or wait for its control immunity to end."); }
    LastError = Error; return bOpened;
}
bool USovAurelionThermalFractureComponent::RequestConfirmFracture(ASovPlayerCharacterBase* Tarrik, FString& Error)
{
    Error.Reset();
    if (bConfirmExecuting || bFrostSetupExecuting || bPayoffPending || bPayoffExecuting || Receipt.IsComplete()
        || !IsContextCurrent(Context, true) || Context.Player.Get() != Tarrik || GetFractureWindowRemainingSeconds() <= 0.f
        || !FMath::IsFinite(HeatConfirmRange) || HeatConfirmRange < 100.f || HeatConfirmRange > 600.f
        || FVector::DistSquared(Tarrik->GetActorLocation(), GetOwner()->GetActorLocation()) > FMath::Square(HeatConfirmRange)
        || !HasLineOfSight(Tarrik, GetOwner()))
    { Error = TEXT("Tarrik must be close to and see the elite during its live frost window."); LastError = Error; return false; }
    const float CurrentPoise = BoundASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetPoiseAttribute());
    if (!FMath::IsFinite(CurrentPoise) || CurrentPoise <= .1f)
    { Error = TEXT("Wait for the elite's Poise to recover, then request a fresh frost setup."); LastError = Error; return false; }
    const FContext Captured = Context; TGuardValue<bool> Confirming(bConfirmExecuting, true);
    auto EffectContext = Captured.PlayerASC->MakeEffectContext(); EffectContext.AddSourceObject(this);
    auto Spec = Captured.PlayerASC->MakeOutgoingSpec(USovGameplayEffect_CinderGrenadeExplosionDamage::StaticClass(), 1.f, EffectContext);
    if (!Spec.IsValid()) { Error = TEXT("The native heat impact could not be prepared."); LastError = Error; return false; }
    const auto& Tags = FSovGameplayTags::Get();
    Spec.Data->AddDynamicAssetTag(TAG_AurelionHeatConfirm); Spec.Data->AddDynamicAssetTag(Tags.Damage_Channel_Thermal);
    Spec.Data->SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Damage, 0.f);
    Spec.Data->SetSetByCallerMagnitude(Tags.SetByCaller_Damage_PoiseDamage, FMath::Min(1.f, CurrentPoise * .1f));
    Captured.PlayerASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), BoundASC.Get());
    const bool bAccepted = IsContextCurrent(Captured, true) && bPayoffPending;
    if (!bAccepted) { Error = TEXT("The elite did not accept Tarrik's native heat impact. Reopen frost after its defense recovers."); }
    LastError = Error; return bAccepted;
}
bool USovAurelionThermalFractureComponent::IsContextCurrent(const FContext& C, bool bRequireActiveSetup) const
{
    if (bEnding || !IsValid(GetOwner()) || GetOwner()->IsActorBeingDestroyed() || !GetOwner()->HasAuthority()
        || !C.Director.IsValid() || C.Director != BoundDirector || C.Director->IsActorBeingDestroyed()
        || (EncounterDirector && EncounterDirector != C.Director.Get()) || C.Director->GetWorld() != GetWorld()
        || C.Director->GetCampaignProofType() != ESovEncounterProofType::AurelionThermalFracture
        || C.Director->EncounterId != C.EncounterId || C.Director->GetAttemptId() != C.AttemptId
        || C.Director->GetLifecycleGeneration() != C.Generation || C.Director->FindParticipantId(GetOwner()).IsNone()
        || !C.Player.IsValid() || !C.Controller.IsValid() || !C.Selene.IsValid()
        || !C.PlayerASC.IsValid() || !C.SeleneASC.IsValid() || !C.TargetASC.IsValid() || C.TargetASC != BoundASC
        || C.Player->IsActorBeingDestroyed() || C.Controller->IsActorBeingDestroyed() || C.Selene->IsActorBeingDestroyed()
        || C.Player->GetWorld() != GetWorld() || C.Selene->GetWorld() != GetWorld()
        || C.Controller->GetPawn() != C.Player.Get() || C.Player->GetController() != C.Controller.Get()
        || C.Player->GetNarrativeAbilitySystemComponent() != C.PlayerASC.Get() || C.PlayerASC->GetAvatarActor() != C.Player.Get()
        || C.Selene->GetNarrativeAbilitySystemComponent() != C.SeleneASC.Get() || C.SeleneASC->GetAvatarActor() != C.Selene.Get()
        || UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()) != C.TargetASC.Get() || C.TargetASC->GetAvatarActor() != GetOwner()
        || !C.Player->IsCharacterReady() || !C.Player->IsAlive() || !C.Selene->IsEncounterSnapshotReady() || !C.Selene->IsAlive()
        // Interruptions reject an open setup/payoff. Subsequent combat actions
        // cannot erase its completed native receipt; ownership/life/attempt
        // checks still apply when querying that receipt.
        || (bRequireActiveSetup && (HeroIsInterrupted(C.PlayerASC.Get()) || HeroIsInterrupted(C.SeleneASC.Get())))
        || C.PlayerASC->GetCharacterReadyEpoch() != C.PlayerReadyEpoch || C.SeleneASC->GetCharacterReadyEpoch() != C.SeleneReadyEpoch
        || C.PlayerASC->GetCombatActorInfoEpoch() != C.PlayerActorInfoEpoch || C.SeleneASC->GetCombatActorInfoEpoch() != C.SeleneActorInfoEpoch
        || C.TargetASC->GetCombatActorInfoEpoch() != C.TargetActorInfoEpoch
        || C.Controller->GetCampaignTransitionEpoch() != C.TransitionEpoch || C.Controller->GetCampaignTransitionState() != ESovCampaignTransitionState::Idle
        || !C.Controller->GetConvergenceCompanionState() || C.Controller->GetConvergenceCompanionState()->GetActiveCompanion() != C.Selene.Get()
        || !C.Campaign.IsValid() || !C.Mission.IsValid() || C.Controller->GetCampaignState() != C.Campaign.Get()
        || C.Campaign->GetActiveMission() != C.Mission.Get() || !C.Controller->GetCampaignState()->IsStateValid()
        || C.Controller->GetCampaignState()->IsMutationInProgress() || !C.Director->HasEncounterPlayer(C.Player.Get())) { return false; }
    const auto& Tags = FSovGameplayTags::Get();
    if (C.Player->GetProtagonistIdentityTag() != Tags.Character_Player_Tarrik
        || C.Controller->GetCampaignState()->GetActiveProtagonist() != Tags.Character_Player_Tarrik
        || C.Selene->GetCompanionIdentity() != Tags.Character_Player_Selene || !C.Selene->GetCompanionComponent()
        || C.Selene->GetCompanionComponent()->GetCurrentLeader() != C.Player.Get()
        || C.Selene->GetCompanionComponent()->IsDisabled()) { return false; }
    const auto State = C.Director->GetEncounterState();
    return (State == ESovEncounterState::Active || (!bRequireActiveSetup && State == ESovEncounterState::Succeeded))
        && (!bRequireActiveSetup || (!C.TargetASC->IsDead()
            && C.TargetASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) > 0.f));
}
void USovAurelionThermalFractureComponent::HandleControlApplied(UAbilitySystemComponent* ASC,
    const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle Handle)
{
    if (bEnding || bPayoffExecuting || Receipt.IsComplete() || ASC != BoundASC.Get() || ObservedControlHandles.Contains(Handle)) { return; }
    const FActiveGameplayEffect* Effect = BoundASC->GetActiveGameplayEffect(Handle);
    if (!Effect || Effect->Spec.GetContext().Get() != Spec.GetContext().Get()
        || !Spec.Def || !Spec.Def->IsA<USovGameplayEffect_SeleneControl>()) { return; }
    FContext Candidate; FString AnchorError;
    if (!CaptureContext(Candidate) || !ValidateCleanAnchor(Candidate, AnchorError)) { return; }
    if (Spec.GetContext().GetOriginalInstigator() != Candidate.Selene.Get()
        || Spec.GetContext().GetOriginalInstigatorAbilitySystemComponent() != Candidate.SeleneASC.Get()) { return; }
    FGameplayTagContainer Granted; Effect->Spec.GetAllGrantedTags(Granted);
    FGameplayTagContainer AssetTags; Effect->Spec.GetAllAssetTags(AssetTags);
    const auto& Tags = FSovGameplayTags::Get();
    const bool bSeleneFrost = AssetTags.HasTagExact(Tags.Ability_Echo_Selene_StillpointGrenade)
        || AssetTags.HasTagExact(Tags.Ability_Echo_Selene_Dispatch) || AssetTags.HasTagExact(Tags.Ability_Echo_Selene_StaccatoZero)
        || AssetTags.HasTagExact(Tags.Ability_Echo_Selene_VeritysWake);
    const bool bFrostApplied = (Granted.HasTagExact(Tags.State_Status_Frozen) && ASC->HasMatchingGameplayTag(Tags.State_Status_Frozen))
        || (Granted.HasTagExact(Tags.State_Status_Chilled) && ASC->HasMatchingGameplayTag(Tags.State_Status_Chilled));
    const float Remaining = Effect->GetTimeRemaining(GetWorld()->GetTimeSeconds());
    const bool bContextSetup = bFrostSetupExecuting && AssetTags.HasTagExact(TAG_AurelionFrostSetup)
        && Spec.GetContext().GetSourceObject() == this;
    if ((!bSeleneFrost && !bContextSetup) || !bFrostApplied || !FMath::IsFinite(Remaining) || Remaining <= 0.f
        || !FMath::IsFinite(FractureWindowSeconds) || FractureWindowSeconds < .25f || FractureWindowSeconds > 10.f) { return; }
    auto* Weak = GetOwner()->FindComponentByClass<USovWeakPointComponent>();
    if (!Weak || !Weak->IsInitialized() || !Weak->HasValidWeakPointConfiguration())
    { LastError = TEXT("Thermal Fracture requires the elite's initialized, authored weak-point zones."); return; }
    // A previously observed live handle can never reopen a missed window through delegate replay.
    for (auto It = ObservedControlHandles.CreateIterator(); It; ++It)
    { if (!BoundASC->GetActiveGameplayEffect(*It)) { It.RemoveCurrent(); } }
    ObservedControlHandles.Add(Handle);
    CloseWindow(); Context = Candidate; FrostApplicationId = FGuid::NewGuid(); FrostHandle = Handle;
    WindowEndsAt = GetWorld()->GetTimeSeconds() + FMath::Min(Remaining, FractureWindowSeconds);
    BindHeroInterruptions();
    const FGuid Opening = FrostApplicationId;
    if (!Weak->RevealWeakPoints(WindowEndsAt - GetWorld()->GetTimeSeconds(), Candidate.Selene.Get())
        || Opening != FrostApplicationId || !IsContextCurrent(Candidate, true))
    { CloseWindow(); return; }
    LastError.Reset();
}
float USovAurelionThermalFractureComponent::GetFractureWindowRemainingSeconds() const
{
    return FrostApplicationId.IsValid() && GetWorld() && IsContextCurrent(Context, true)
        && BoundASC->GetActiveGameplayEffect(FrostHandle)
        ? FMath::Max(0.f, WindowEndsAt - GetWorld()->GetTimeSeconds()) : 0.f;
}
void USovAurelionThermalFractureComponent::HandleDamage(const FSovDamageResult& Result)
{
    if (bEnding || Result.TargetActor != GetOwner() || !Result.HasNativeReceipt() || !Result.IsCurrentTargetLife(BoundASC.Get())) { return; }
    if (bPayoffExecuting)
    {
        if (ExpectedPayoffContext && Result.EffectContext.Get() == ExpectedPayoffContext
            && Result.EffectContext.GetSourceObject() == this && Result.SourceActor == Context.Player.Get()
            && Result.AppliedPoiseDamage > 0.f && Result.bPoiseBroken && !Result.bFatal
            && Result.AppliedHealthDamage == 0.f && Result.AppliedShieldDamage == 0.f && Result.ConsumeNativeReceipt(this, 1))
        { ObservedPayoffId = Result.TransactionId; bObservedPoiseBreak = true; }
        return;
    }
    if (Receipt.IsComplete() || bPayoffPending || GetFractureWindowRemainingSeconds() <= 0.f) { return; }
    const auto& Tags = FSovGameplayTags::Get();
    if (Result.SourceActor != Context.Player.Get() || Result.bPeriodicDamage || Result.bFatal
        || Result.DefenseKind != ESovDefenseKind::None || !Result.DamageChannels.HasTagExact(Tags.Damage_Channel_Thermal)
        || Result.RejectedDamageChannels.HasTagExact(Tags.Damage_Channel_Thermal)
        || !(Result.AppliedHealthDamage > 0.f || Result.AppliedShieldDamage > 0.f || Result.AppliedPoiseDamage > 0.f)
        || !Result.ConsumeNativeReceipt(this)) { return; }
    const FContext Captured = Context; const FGuid FrostId = FrostApplicationId; const FGuid HeatId = Result.TransactionId;
    bPayoffPending = true;
    // Leave the heat transaction's mutation scope before executing another native damage operation.
    PayoffTimer = GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,
        [this, Captured, FrostId, HeatId]() { ExecuteFracture(Captured, FrostId, HeatId); }));
}
void USovAurelionThermalFractureComponent::ExecuteFracture(FContext Captured, FGuid FrostId, FGuid HeatId)
{
    if (!bPayoffPending || bEnding || FrostId != FrostApplicationId) { return; }
    bPayoffPending = false;
    if (!IsContextCurrent(Captured, true) || GetFractureWindowRemainingSeconds() <= 0.f) { CloseWindow(); return; }
    auto* Poise = GetOwner()->FindComponentByClass<USovPoiseComponent>();
    const auto& Tags = FSovGameplayTags::Get();
    if (!Poise || !Poise->IsInitialized() || !(Poise->GetPoise() > 0.f)
        || BoundASC->HasMatchingGameplayTag(Tags.State_InterruptProtected)
        || BoundASC->HasMatchingGameplayTag(Tags.State_Poise_SuperArmor) || Poise->IsPoiseRecovering())
    { LastError = TEXT("Thermal Fracture had no eligible Poise break. A fresh frost application can reopen the window."); CloseWindow(); return; }
    TGuardValue<bool> Executing(bPayoffExecuting, true); ObservedPayoffId.Invalidate(); bObservedPoiseBreak = false;
    FGameplayEffectContextHandle EffectContext = Captured.PlayerASC->MakeEffectContext(); EffectContext.AddSourceObject(this);
    auto Spec = Captured.PlayerASC->MakeOutgoingSpec(USovGameplayEffect_CinderGrenadeExplosionDamage::StaticClass(), 1.f, EffectContext);
    if (!Spec.IsValid()) { CloseWindow(); return; }
    Spec.Data->AddDynamicAssetTag(Tags.Damage_Channel_Thermal);
    Spec.Data->AddDynamicAssetTag(Tags.Damage_BypassGuard); Spec.Data->AddDynamicAssetTag(Tags.Damage_BypassDeflection);
    Spec.Data->SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Damage, 0.f);
    Spec.Data->SetSetByCallerMagnitude(Tags.SetByCaller_Damage_PoiseDamage, Poise->GetPoise());
    ExpectedPayoffContext = EffectContext.Get();
    Captured.PlayerASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), BoundASC.Get());
    ExpectedPayoffContext = nullptr;
    if (!IsContextCurrent(Captured, true) || FrostId != FrostApplicationId || !bObservedPoiseBreak
        || !ObservedPayoffId.IsValid() || !Poise->IsPoiseBroken())
    { LastError = TEXT("Thermal Fracture did not produce a current native Poise break. Reapply frost to retry."); CloseWindow(); return; }
    Receipt.EncounterId = Captured.EncounterId; Receipt.AttemptId = Captured.AttemptId;
    Receipt.FrostApplicationId = FrostId; Receipt.HeatTransactionId = HeatId; Receipt.PayoffTransactionId = ObservedPayoffId;
    CloseWindow(); LastError.Reset();
    const FSovAurelionThermalFractureReceipt Published = Receipt;
    // The frost mark is temporary mission intent. Release only that still-
    // accepted hold after the real payoff; preserve any newer player command.
    if (auto* Companion = Captured.Selene->GetCompanionComponent(); Companion
        && Companion->GetCurrentLeader() == Captured.Player.Get()
        && Companion->HasAcceptedHoldPosition(FrostAnchor))
    {
        FString CommandError;
        Companion->RequestCommand(Captured.Player.Get(), ESovCompanionCommand::Regroup, Captured.Player.Get(), CommandError);
    }
    OnThermalFractureCompleted.Broadcast(Published);
}
bool USovAurelionThermalFractureComponent::HasCompletedFracture(const ASovEncounterDirector* Director, const FGuid& AttemptId) const
{
    return Receipt.IsComplete() && Director == BoundDirector.Get() && Receipt.AttemptId == AttemptId
        && IsContextCurrent(Context, false);
}
bool USovAurelionThermalFractureComponent::HasCompletedCurrentFracture() const
{
    const ASovEncounterDirector* Director = BoundDirector.Get();
    return Director && HasCompletedFracture(Director, Director->GetAttemptId());
}
void USovAurelionThermalFractureComponent::HandleEncounterState(ESovEncounterState Previous, ESovEncounterState Current)
{
    if (!BoundDirector.IsValid() || BoundDirector->GetEncounterState() != Current) { return; }
    if (Current == ESovEncounterState::Succeeded) { CloseWindow(); return; }
    Retire();
}
void USovAurelionThermalFractureComponent::CloseWindow()
{
    for (const auto& Binding : InterruptBindings)
    {
        if (Binding.ASC.IsValid())
        { Binding.ASC->RegisterGameplayTagEvent(Binding.Tag, EGameplayTagEventType::NewOrRemoved).Remove(Binding.Handle); }
    }
    InterruptBindings.Reset();
    if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(PayoffTimer); }
    bPayoffPending = false; FrostApplicationId.Invalidate(); FrostHandle.Invalidate(); WindowEndsAt = 0.f;
}
void USovAurelionThermalFractureComponent::BindHeroInterruptions()
{
    for (auto* ASC : {Context.PlayerASC.Get(), Context.SeleneASC.Get()})
    {
        if (!ASC) { continue; }
        for (const auto Tag : HeroInterruptTags())
        {
            FInterruptBinding Binding; Binding.ASC = ASC; Binding.Tag = Tag;
            Binding.Handle = ASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved)
                .AddUObject(this, &ThisClass::HandleHeroInterrupt);
            InterruptBindings.Add(Binding);
        }
    }
}
void USovAurelionThermalFractureComponent::HandleHeroInterrupt(FGameplayTag Tag, int32 Count)
{
    if (Count <= 0 || Receipt.IsComplete()) { return; }
    CloseWindow(); LastError = TEXT("The frost setup was interrupted. Request a fresh setup after both heroes recover.");
}
void USovAurelionThermalFractureComponent::Retire()
{ CloseWindow(); Receipt = {}; SavedReceipt = {}; Context = {}; ObservedControlHandles.Reset(); NextFrostSetupAt = 0.f; }
void USovAurelionThermalFractureComponent::PrepareForSave_Implementation()
{ SavedReceipt = HasCompletedFracture(BoundDirector.Get(), Receipt.AttemptId) ? Receipt : FSovAurelionThermalFractureReceipt(); }
void USovAurelionThermalFractureComponent::Load_Implementation()
{ Retire(); InitializeBindings(); }
bool USovAurelionThermalFractureComponent::LoadMissingSaveRecord()
{ Retire(); return true; }
void USovAurelionThermalFractureComponent::Unbind()
{
    if (BoundASC.IsValid())
    { BoundASC->OnActiveGameplayEffectAddedDelegateToSelf.Remove(ControlDelegate); BoundASC->OnDamageResolvedAsTarget.RemoveDynamic(this, &ThisClass::HandleDamage); }
    if (BoundDirector.IsValid()) { BoundDirector->OnEncounterStateChanged.RemoveDynamic(this, &ThisClass::HandleEncounterState); }
    BoundASC.Reset(); BoundDirector.Reset(); ControlDelegate.Reset();
}
void USovAurelionThermalFractureComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    bEnding = true; Retire(); Unbind();
    if (auto* Character = Cast<ANarrativeCharacter>(GetOwner())) { Character->OnASCInitialized.RemoveDynamic(this, &ThisClass::HandleOwnerASCInitialized); }
    Super::EndPlay(Reason);
}
