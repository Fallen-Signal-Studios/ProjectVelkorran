// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Presentation/SovCombatFeedbackComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sovereign/SovGameplayTags.h"
#include "Settings/SovGameUserSettings.h"
#include "TimerManager.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "UObject/UnrealType.h"

namespace
{
    // Only this presenter registers entries. Weak world/component references cannot retain a retired world.
    TArray<TWeakObjectPtr<UNiagaraComponent>> WorldBursts;
    int32 CountWorldBursts(UWorld* World)
    {
        WorldBursts.RemoveAll([](const auto& Entry) { return !Entry.IsValid() || !Entry->IsRegistered(); });
        int32 Count = 0;
        for (const auto& Entry : WorldBursts) { Count += Entry->GetWorld() == World ? 1 : 0; }
        return Count;
    }
    // Respect an existing authored shield presentation without changing that component or its delegates.
    bool HasExistingShieldBurst(AActor* Target)
    {
        if (!IsValid(Target)) { return false; }
        for (UActorComponent* Component : Target->GetComponents())
        {
            if (const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Component->GetClass(), TEXT("ShieldBreakSystem")))
            {
                if (IsValid(Property->GetObjectPropertyValue_InContainer(Component))) { return true; }
            }
        }
        return false;
    }
}

ESovCombatFeedback FSovCombatFeedbackPolicy::Select(const FSovDamageResult& Result, bool bSelene)
{
    // Break wins over routine impact. A requested status, blocked attack, or periodic tick is not a hit burst.
    if ((Result.bShieldBroken && Result.AppliedShieldDamage > 0.f)
        || (Result.bPoiseBroken && Result.AppliedPoiseDamage > 0.f)) { return ESovCombatFeedback::Break; }
    if (Result.bPeriodicDamage || Result.DefenseKind != ESovDefenseKind::None
        || Result.AppliedHealthDamage + Result.AppliedShieldDamage <= 0.f) { return ESovCombatFeedback::None; }
    return bSelene ? ESovCombatFeedback::SeleneImpact : ESovCombatFeedback::TarrikImpact;
}

bool FSovCombatFeedbackPolicy::Admit(double Now, double& Last, ESovCombatFeedback Kind, int32 LocalCount, int32 WorldCount)
{
    if (Kind == ESovCombatFeedback::None || !FMath::IsFinite(Now)) { return false; }
    const bool Critical = IsCritical(Kind);
    if (LocalCount >= (Critical ? LocalTotalLimit : LocalRoutineLimit)
        || WorldCount >= (Critical ? WorldTotalLimit : WorldRoutineLimit)
        || Now - Last < (Critical ? .10 : .08)) { return false; }
    Last = Now;
    return true;
}

USovCombatFeedbackComponent::USovCombatFeedbackComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
    for (const TCHAR* Name : {TEXT("TarrikImpact"), TEXT("SeleneImpact"), TEXT("Break")})
    {
        const FString Base = FString(TEXT("/Game/Aurelion/VFX/NS_Aurelion_")) + Name;
        NormalSystems.Add(TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(Base + TEXT(".NS_Aurelion_") + Name)));
        ReducedSystems.Add(TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(Base + TEXT("_Reduced.NS_Aurelion_") + Name + TEXT("_Reduced"))));
    }
}

void USovCombatFeedbackComponent::BeginPlay()
{
    Super::BeginPlay();
    if (ANarrativeCharacter* Character = Cast<ANarrativeCharacter>(GetOwner()))
    { Character->OnASCInitialized.AddUniqueDynamic(this, &ThisClass::HandleASCInitialized); }
    HandleASCInitialized();
    if (GetNetMode() != NM_DedicatedServer)
    {
        TArray<FSoftObjectPath> Paths;
        for (const auto& Asset : NormalSystems) { Paths.AddUnique(Asset.ToSoftObjectPath()); }
        for (const auto& Asset : ReducedSystems) { Paths.AddUnique(Asset.ToSoftObjectPath()); }
        LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths);
    }
}

void USovCombatFeedbackComponent::Unbind()
{
    if (IsValid(BoundASC))
    {
        BoundASC->OnDamageResolvedAsSource.RemoveDynamic(this, &ThisClass::HandleOutgoing);
        BoundASC->OnDamageResolvedAsTarget.RemoveDynamic(this, &ThisClass::HandleIncoming);
    }
    BoundASC = nullptr;
}

void USovCombatFeedbackComponent::HandleASCInitialized()
{
    if (bEndingPlay) { return; }
    auto* ASC = Cast<UNarrativeAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()));
    if (ASC == BoundASC && HasCurrentBinding()) { return; }
    Unbind();
    if (!IsValid(ASC) || ASC->GetAvatarActor() != GetOwner() || !IsValid(GetOwner()) || GetOwner()->IsActorBeingDestroyed()) { return; }
    BoundASC = ASC;
    BoundASC->OnDamageResolvedAsSource.AddUniqueDynamic(this, &ThisClass::HandleOutgoing);
    BoundASC->OnDamageResolvedAsTarget.AddUniqueDynamic(this, &ThisClass::HandleIncoming);
}

bool USovCombatFeedbackComponent::HasCurrentBinding() const
{
    return !bEndingPlay && IsValid(GetOwner()) && !GetOwner()->IsActorBeingDestroyed() && IsValid(BoundASC)
        && BoundASC->GetAvatarActor() == GetOwner()
        && UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()) == BoundASC;
}

bool USovCombatFeedbackComponent::HasAdmissibleNativeReceipt(const FSovDamageResult& Result)
{
    return Result.TransactionId.IsValid() && Result.HasNativeReceipt() && Result.IsCurrentTargetLife()
        && IsValid(Result.SourceActor) && IsValid(Result.TargetActor);
}

void USovCombatFeedbackComponent::HandleOutgoing(const FSovDamageResult& Result) { Dispatch(Result, false); }
void USovCombatFeedbackComponent::HandleIncoming(const FSovDamageResult& Result) { Dispatch(Result, true); }

void USovCombatFeedbackComponent::Dispatch(const FSovDamageResult& Result, bool bIncoming)
{
    if (!HasCurrentBinding() || !GetOwner()->HasAuthority() || !HasAdmissibleNativeReceipt(Result)
        || (bIncoming ? Result.TargetActor.Get() : Result.SourceActor.Get()) != GetOwner()) { return; }
    const auto& Tags = FSovGameplayTags::Get();
    const bool Selene = BoundASC->HasMatchingGameplayTag(Tags.Character_Player_Selene);
    if (!Selene && !BoundASC->HasMatchingGameplayTag(Tags.Character_Player_Tarrik)) { return; }
    const ESovCombatFeedback Kind = FSovCombatFeedbackPolicy::Select(Result, Selene);
    if (Kind == ESovCombatFeedback::None) { return; }
    // A protagonist target owns its break. Enemy breaks are presented by the protagonist source.
    if (bIncoming && Kind != ESovCombatFeedback::Break) { return; }
    if (!bIncoming && Kind == ESovCombatFeedback::Break
        && Result.TargetActor->FindComponentByClass<USovCombatFeedbackComponent>()) { return; }
    if (Kind == ESovCombatFeedback::Break && Result.bShieldBroken && HasExistingShieldBurst(Result.TargetActor)) { return; }
    if (!Result.ConsumeNativeReceipt(this)) { return; }
    double& Last = FSovCombatFeedbackPolicy::IsCritical(Kind) ? LastAuthorityCritical : LastAuthorityRoutine;
    // Network burst budget is separate from local renderer budget, including on a listen server.
    if (!FSovCombatFeedbackPolicy::Admit(GetWorld()->GetTimeSeconds(), Last, Kind, 0, 0)) { ++BudgetDropCount; return; }
    FVector Position = Result.TargetActor->GetActorLocation();
    FVector Normal = (Result.SourceActor->GetActorLocation() - Position).GetSafeNormal(SMALL_NUMBER, FVector::ForwardVector);
    if (const FHitResult* Hit = Result.EffectContext.GetHitResult(); Hit && Hit->GetActor() == Result.TargetActor)
    { Position = Hit->ImpactPoint; Normal = Hit->ImpactNormal.GetSafeNormal(SMALL_NUMBER, FVector::ForwardVector); }
    if (Position.ContainsNaN() || Normal.ContainsNaN()) { return; }
    MulticastFeedback(Kind, Position + Normal * 4.f, Normal);
}

bool USovCombatFeedbackComponent::IsReducedCombatEffects() const
{
    const UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
    const auto* Preferences = Cast<USovGameUserSettings>(Settings);
    return bReducedCombatEffects || (Preferences && Preferences->IsReducedCombatEffectsEnabled())
        || (Settings && Settings->GetVisualEffectQuality() == 0);
}

int32 USovCombatFeedbackComponent::GetLiveBurstCount() const
{
    int32 Count = 0;
    for (const auto& Entry : LiveBursts) { Count += Entry.Component.IsValid() ? 1 : 0; }
    return Count;
}

void USovCombatFeedbackComponent::MulticastFeedback_Implementation(ESovCombatFeedback Kind, FVector_NetQuantize Location, FVector_NetQuantizeNormal Normal)
{
    UWorld* World = GetWorld();
    const int32 Index = static_cast<int32>(Kind) - 1;
    if (bEndingPlay || !IsValid(World) || GetNetMode() == NM_DedicatedServer || !NormalSystems.IsValidIndex(Index)
        || Location.ContainsNaN() || Normal.ContainsNaN()) { return; }
    const bool Reduced = IsReducedCombatEffects();
    UNiagaraSystem* System = Reduced ? ReducedSystems[Index].Get() : NormalSystems[Index].Get();
    // No synchronous load in an impact callback. Missing reduced data falls back to the complete signal.
    const bool UsedReduced = Reduced && IsValid(System);
    if (!IsValid(System)) { System = NormalSystems[Index].Get(); }
    if (!IsValid(System) || !System->IsReadyToRun()) { return; }
    const bool Critical = FSovCombatFeedbackPolicy::IsCritical(Kind);
    for (auto It = World->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (!PC || !PC->IsLocalController()) { continue; }
        FVector View; FRotator ViewRotation; PC->GetPlayerViewPoint(View, ViewRotation);
        if (FVector::DistSquared(View, Location) > FMath::Square(Critical ? 6000.f : 3500.f)) { return; }
        break;
    }
    double& Last = Critical ? LastLocalCritical : LastLocalRoutine;
    if (!FSovCombatFeedbackPolicy::Admit(World->GetTimeSeconds(), Last, Kind, GetLiveBurstCount(), CountWorldBursts(World)))
    { ++BudgetDropCount; return; }
    // Small surface-anchored bursts, no pooled component reuse and no persistent cloud.
    UNiagaraComponent* Burst = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, System, Location,
        FVector(Normal).Rotation(), FVector::OneVector, false, false, ENCPoolMethod::None, true);
    if (!IsValid(Burst)) { return; }
    Burst->SetCanEverAffectNavigation(false);
    Burst->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Burst->SetCastShadow(false);
    FLiveBurst Entry; Entry.Component = Burst; Entry.bCritical = Critical;
    const TWeakObjectPtr<UNiagaraComponent> WeakBurst(Burst);
    World->GetTimerManager().SetTimer(Entry.Timer, FTimerDelegate::CreateWeakLambda(this,
        [this, WeakBurst]() { RetireBurst(WeakBurst); }), Critical ? .65f : .40f, false);
    LiveBursts.Add(Entry); WorldBursts.Add(Burst);
    Burst->Activate(true);
    if (!IsValid(Burst) || !Burst->IsActive() || bEndingPlay)
    { if (IsValid(World)) { World->GetTimerManager().ClearTimer(Entry.Timer); } RetireBurst(WeakBurst); return; }
    ++PresentedCount;
    // Observers receive only an actually activated component, never a requested or unloaded effect.
    OnFeedbackPresented.Broadcast(Kind, Location, UsedReduced);
}

void USovCombatFeedbackComponent::RetireBurst(TWeakObjectPtr<UNiagaraComponent> Burst)
{
    if (Burst.IsValid()) { Burst->DeactivateImmediate(); Burst->DestroyComponent(); }
    LiveBursts.RemoveAll([Burst](const FLiveBurst& Entry) { return Entry.Component == Burst; });
}

void USovCombatFeedbackComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    bEndingPlay = true;
    if (ANarrativeCharacter* Character = Cast<ANarrativeCharacter>(GetOwner()))
    { Character->OnASCInitialized.RemoveDynamic(this, &ThisClass::HandleASCInitialized); }
    Unbind();
    if (LoadHandle.IsValid()) { LoadHandle->CancelHandle(); LoadHandle.Reset(); }
    for (auto& Entry : LiveBursts)
    {
        if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(Entry.Timer); }
        if (Entry.Component.IsValid()) { Entry.Component->DeactivateImmediate(); Entry.Component->DestroyComponent(); }
    }
    LiveBursts.Reset();
    Super::EndPlay(Reason);
}
