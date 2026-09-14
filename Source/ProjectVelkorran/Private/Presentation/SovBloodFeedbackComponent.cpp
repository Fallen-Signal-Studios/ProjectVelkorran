#include "Presentation/SovBloodFeedbackComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Settings/SovGameUserSettings.h"
#include "Sovereign/SovGameplayTags.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "TimerManager.h"

namespace { TArray<TWeakObjectPtr<UNiagaraComponent>> BloodBursts; }

USovBloodFeedbackComponent::USovBloodFeedbackComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
    for (const TCHAR* Color : {TEXT("Red"), TEXT("Black")})
        for (const TCHAR* Kind : {TEXT("Hit"), TEXT("Slash"), TEXT("Burst"), TEXT("Low")})
        {
            const FString Name = FString(TEXT("NS_Aurelion_Blood")) + Color + Kind;
            Systems.Add(TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Aurelion/VFX/Blood/") + Name + TEXT(".") + Name)));
        }
}

bool USovBloodFeedbackComponent::ShouldPresent(const FSovDamageResult& R)
{
    return R.AppliedHealthDamage > 0.f && FMath::IsFinite(R.AppliedHealthDamage)
        && !R.bPeriodicDamage && R.DefenseKind == ESovDefenseKind::None;
}

void USovBloodFeedbackComponent::BeginPlay()
{
    Super::BeginPlay();
    if (!bEnabled) { return; }
    if (auto* Character = Cast<ANarrativeCharacter>(GetOwner()))
        Character->OnASCInitialized.AddUniqueDynamic(this, &ThisClass::BindASC);
    BindASC();
    if (GetNetMode() != NM_DedicatedServer)
    {
        TArray<FSoftObjectPath> Paths;
        for (int32 Index = bBlackBlood ? 4 : 0; Index < (bBlackBlood ? 8 : 4); ++Index)
            Paths.Add(Systems[Index].ToSoftObjectPath());
        LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths);
    }
}

void USovBloodFeedbackComponent::BindASC()
{
    if (BoundASC) { BoundASC->OnDamageResolvedAsTarget.RemoveDynamic(this, &ThisClass::OnDamage); }
    BoundASC = Cast<UNarrativeAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()));
    if (BoundASC && BoundASC->GetAvatarActor() == GetOwner())
        BoundASC->OnDamageResolvedAsTarget.AddUniqueDynamic(this, &ThisClass::OnDamage);
}

void USovBloodFeedbackComponent::OnDamage(const FSovDamageResult& R)
{
    if (!bEnabled || !GetOwner()->HasAuthority() || R.TargetActor != GetOwner() || !ShouldPresent(R)
        || !BoundASC || BoundASC->GetAvatarActor() != GetOwner()
        || UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()) != BoundASC
        || !R.TransactionId.IsValid() || !R.HasNativeReceipt() || !R.IsCurrentTargetLife()
        || !R.ConsumeNativeReceipt(this)) { return; }
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now - LastBurst < .075) { return; }
    FVector Position = GetOwner()->GetActorLocation();
    FVector Normal = IsValid(R.SourceActor) ? (R.SourceActor->GetActorLocation() - Position).GetSafeNormal() : FVector::UpVector;
    if (const FHitResult* Hit = R.EffectContext.GetHitResult(); Hit && Hit->GetActor() == GetOwner())
    { Position = Hit->ImpactPoint; Normal = Hit->ImpactNormal; }
    if (Position.ContainsNaN() || Normal.ContainsNaN()) { return; }
    LastBurst = Now;
    const uint8 Kind = R.HealthOverkillDamage > 0.f || R.AppliedHealthDamage >= 60.f ? 2
        : R.DamageChannels.HasTag(FSovGameplayTags::Get().Damage_Channel_Edge) ? 1 : 0;
    MulticastBlood(Kind, Position + Normal * 2.f, Normal.GetSafeNormal(SMALL_NUMBER, FVector::UpVector));
}

void USovBloodFeedbackComponent::MulticastBlood_Implementation(uint8 Kind, FVector_NetQuantize Position, FVector_NetQuantizeNormal Normal)
{
    if (!bEnabled || GetNetMode() == NM_DedicatedServer || Kind > 2 || !GetWorld()) { return; }
    BloodBursts.RemoveAll([](const auto& Entry) { return !Entry.IsValid(); });
    int32 Count = 0;
    for (const auto& Entry : BloodBursts) { Count += Entry->GetWorld() == GetWorld() ? 1 : 0; }
    if (Count >= 24) { return; }
    const auto* Settings = Cast<USovGameUserSettings>(UGameUserSettings::GetGameUserSettings());
    const bool Reduced = Settings && (Settings->IsReducedCombatEffectsEnabled() || Settings->GetVisualEffectQuality() == 0);
    UNiagaraSystem* System = Systems[(bBlackBlood ? 4 : 0) + (Reduced ? 3 : Kind)].Get();
    if (!IsValid(System) || !System->IsReadyToRun()) { return; }
    auto* Burst = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), System, Position,
        FVector(Normal).Rotation(), FVector::OneVector, true, true, ENCPoolMethod::None, true);
    if (!Burst) { return; }
    Burst->SetCanEverAffectNavigation(false);
    BloodBursts.Add(Burst);
    // Finite lifetime even if an authored emitter accidentally loops. World timer does not retain the victim.
    const TWeakObjectPtr<UNiagaraComponent> Weak(Burst);
    FTimerHandle Timer;
    GetWorld()->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateLambda([Weak]()
        { if (Weak.IsValid()) { Weak->DestroyComponent(); } }), 8.f, false);
}

void USovBloodFeedbackComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (BoundASC) { BoundASC->OnDamageResolvedAsTarget.RemoveDynamic(this, &ThisClass::OnDamage); }
    if (auto* Character = Cast<ANarrativeCharacter>(GetOwner()))
        Character->OnASCInitialized.RemoveDynamic(this, &ThisClass::BindASC);
    BoundASC = nullptr;
    if (LoadHandle) { LoadHandle->CancelHandle(); LoadHandle.Reset(); }
    Super::EndPlay(Reason);
}
