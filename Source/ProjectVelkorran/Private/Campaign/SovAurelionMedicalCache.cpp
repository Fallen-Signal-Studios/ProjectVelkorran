// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovAurelionMedicalCache.h"
#include "Campaign/SovAurelionPrioritySupport.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/SovPlayerController.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/SecureHash.h"
#include "NarrativeGameplayTags.h"
#include "UObject/StrongObjectPtr.h"

#define LOCTEXT_NAMESPACE "SovAurelionSupplies"
namespace { const FName MedicalMagnitude(TEXT("AurelionMedicalHeal")); }
USovAurelionMedicalAid::USovAurelionMedicalAid()
{
    DurationPolicy = EGameplayEffectDurationType::Instant;
    FGameplayModifierInfo Modifier; Modifier.Attribute = UNarrativeAttributeSetBase::GetHealthAttribute();
    Modifier.ModifierOp = EGameplayModOp::Additive;
    FSetByCallerFloat Amount; Amount.DataName = MedicalMagnitude;
    Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Amount); Modifiers.Add(Modifier);
}
bool USovAurelionMedicalInteraction::CanInteract_Implementation(APawn* Pawn, UNarrativeInteractionComponent* Interaction, FText& Error)
{
    const auto* Cache = Cast<ASovAurelionMedicalCache>(GetOwner());
    return Cache && Super::CanInteract_Implementation(Pawn, Interaction, Error) && Cache->CanUse(Pawn, Error);
}
bool USovAurelionMedicalInteraction::Interact(APawn* Pawn, UNarrativeInteractionComponent* Interaction)
{
    auto* Cache = Cast<ASovAurelionMedicalCache>(GetOwner()); FText Error;
    if (!Cache || !CanInteract(Pawn, Interaction, Error) || !Cache->TryUse(Pawn, Error)) { return false; }
    OnInteract(Pawn, Interaction);
    if (IsValid(this) && !IsBeingDestroyed()) { OnInteracted.Broadcast(Pawn, Interaction); }
    return true;
}
ASovAurelionMedicalCache::ASovAurelionMedicalCache()
{
    PrimaryActorTick.bCanEverTick = true; PrimaryActorTick.TickInterval = .2f;
    Body = CreateDefaultSubobject<UBoxComponent>(TEXT("Body")); SetRootComponent(Body);
    Body->SetBoxExtent(FVector(50,40,45)); Body->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual")); Visual->SetupAttachment(Body);
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label")); Label->SetupAttachment(Body);
    Label->SetRelativeLocation(FVector(0,0,75)); Label->SetWorldSize(16); Label->SetHorizontalAlignment(EHTA_Center);
    Interactable = CreateDefaultSubobject<USovAurelionMedicalInteraction>(TEXT("Interactable"));
    Interactable->InteractionDistance = 250; Interactable->InteractionTime = .35f;
    Interactable->InteractableNameText = LOCTEXT("MedicalCache", "West medical cache");
    Interactable->InteractableActionText = LOCTEXT("UseAid", "Use medical aid");
}
bool ASovAurelionMedicalCache::CanUse(const APawn* Pawn, FText& Error) const
{
    Error = FText::GetEmpty();
    const auto Fail = [&Error](const FText& Text) { Error = Text; return false; };
    const auto* Player = Cast<ASovPlayerCharacterBase>(Pawn);
    const auto* PC = IsValid(Player) ? Cast<ASovPlayerController>(Player->GetController()) : nullptr;
    const auto* ASC = IsValid(Player) ? Player->GetNarrativeAbilitySystemComponent() : nullptr;
    const auto* Campaign = IsValid(PC) ? PC->GetCampaignState() : nullptr;
    if (!HasAuthority() || GetNetMode()!=NM_Standalone || bMutating || IsActorBeingDestroyed() || CacheId.IsNone()
        || !IsValid(Player) || Player->IsActorBeingDestroyed() || Player->GetWorld()!=GetWorld() || !IsValid(PC)
        || PC->GetPawn()!=Player || !IsValid(ASC) || ASC->GetAvatarActor()!=Player || !Player->IsCharacterReady() || !Player->IsAlive()
        || PC->GetCampaignTransitionState()!=ESovCampaignTransitionState::Idle || !IsValid(Campaign) || !Campaign->IsStateValid()
        || Campaign->IsMutationInProgress() || !Campaign->GetActiveMission() || Campaign->GetActiveMission()->MissionId!=TEXT("M12_FireAndFrost")
        || Campaign->GetActiveProtagonist()!=Player->GetProtagonistIdentityTag() || !IsValid(Interactable)
        || !Interactable->IsRegistered() || !Interactable->IsActive()) { return Fail(LOCTEXT("Unavailable", "Medical aid unavailable")); }
    if (bConsumed) { return Fail(LOCTEXT("Empty", "Cache empty")); }
    if (!IsValid(Support) || Support->IsActorBeingDestroyed() || Support->GetWorld()!=GetWorld() || !Support->IsWestCacheAccessible())
    { return Fail(LOCTEXT("WestOnly", "Available after the west stretcher priority")); }
    int32 Supports=0;
    for (TActorIterator<ASovAurelionPrioritySupport> It(GetWorld()); It; ++It) { if (!It->IsActorBeingDestroyed()) { ++Supports; } }
    if (Supports!=1) { return Fail(LOCTEXT("AmbiguousSupport", "Medical support is not configured")); }
    for (TActorIterator<ASovAurelionMedicalCache> It(GetWorld()); It; ++It)
    { if (*It!=this && !It->IsActorBeingDestroyed() && It->CacheId==CacheId) { return Fail(LOCTEXT("AmbiguousCache", "Medical cache is not configured")); } }
    const auto& NarrativeTags=FNarrativeGameplayTags::Get();
    if (ASC->HasMatchingGameplayTag(NarrativeTags.State_Busy) || ASC->HasMatchingGameplayTag(NarrativeTags.State_SequencerControlled)
        || ASC->HasMatchingGameplayTag(NarrativeTags.State_DialogueControlled) || ASC->HasMatchingGameplayTag(NarrativeTags.State_Movement_Ragdoll))
    { return Fail(LOCTEXT("Busy", "Finish the current action first")); }
    const float Maximum=ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetMaxHealthAttribute());
    const float Health=ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute());
    if (!FMath::IsFinite(Maximum) || !FMath::IsFinite(Health) || Maximum<=0 || Health<=0 || Health>=Maximum
        || !FMath::IsFinite(HealthFraction) || HealthFraction<=0 || HealthFraction>1) { return Fail(LOCTEXT("Full", "Health is already full")); }
    if (Player->GetActorLocation().ContainsNaN() || GetActorLocation().ContainsNaN()
        || FVector::DistSquared(Player->GetActorLocation(),GetActorLocation())>FMath::Square(250.f))
    { return Fail(LOCTEXT("Closer", "Move closer to the cache")); }
    FHitResult Hit; FCollisionQueryParams Query=Player->GetIgnoreCharacterParams(); Query.AddIgnoredActor(this);
    if (GetWorld()->LineTraceSingleByChannel(Hit,Player->GetPawnViewLocation(),GetActorLocation(),ECC_Visibility,Query))
    { return Fail(LOCTEXT("Blocked", "The medical cache is obstructed")); }
    return true;
}
bool ASovAurelionMedicalCache::TryUse(APawn* Pawn, FText& Error)
{
    if (!CanUse(Pawn,Error)) { return false; }
    auto* Player=CastChecked<ASovPlayerCharacterBase>(Pawn); auto* ASC=Player->GetNarrativeAbilitySystemComponent();
    FGameplayEffectSpecHandle Spec=ASC->MakeOutgoingSpec(USovAurelionMedicalAid::StaticClass(),1,ASC->MakeEffectContext());
    if (!Spec.IsValid() || !CanUse(Pawn,Error)) { return false; }
    Spec.Data->SetSetByCallerMagnitude(MedicalMagnitude,ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetMaxHealthAttribute())*HealthFraction);
    TStrongObjectPtr<ASovAurelionMedicalCache> KeepCache(this); TStrongObjectPtr<ASovPlayerCharacterBase> KeepPlayer(Player);
    TGuardValue<bool> Mutation(bMutating,true);
    // Reserve before synchronous GAS callbacks; an in-progress record fails save serialization.
    bConsumed=true;
    const auto Applied=ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
    if (!Applied.WasSuccessfullyApplied()) { bConsumed=false; return false; }
    if (!IsActorBeingDestroyed()) { RefreshPresentation(); }
    return true;
}
FGuid ASovAurelionMedicalCache::GetActorGUID_Implementation() const
{
    if (!SaveGuid.IsValid())
    {
        FString Path=GetPathName(); if (GetWorld() && !GetWorld()->StreamingLevelsPrefix.IsEmpty()) { Path.ReplaceInline(*GetWorld()->StreamingLevelsPrefix,TEXT("")); }
        FGuid Stable; FGuid::ParseExact(FMD5::HashAnsiString(*Path),EGuidFormats::Digits,Stable);
        const_cast<ASovAurelionMedicalCache*>(this)->SaveGuid=Stable;
    }
    return SaveGuid;
}
void ASovAurelionMedicalCache::Serialize(FArchive& Ar)
{
    Super::Serialize(Ar);
    if (Ar.IsSaveGame() && Ar.IsSaving() && bMutating) { Ar.SetError(); }
}
void ASovAurelionMedicalCache::RefreshPresentation()
{
    Label->SetText(bConsumed ? LOCTEXT("Spent", "MEDICAL CACHE — EMPTY") : FText::Format(LOCTEXT("AvailableAid", "MEDICAL AID\nOne use · restores {0} health"),FText::AsPercent(HealthFraction)));
    Visual->SetVisibility(!bConsumed);
    Body->SetCollisionEnabled(bConsumed ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
}
void ASovAurelionMedicalCache::BeginPlay() { Super::BeginPlay(); RefreshPresentation(); }
void ASovAurelionMedicalCache::Load_Implementation() { RefreshPresentation(); }
void ASovAurelionMedicalCache::Tick(float DeltaSeconds) { Super::Tick(DeltaSeconds); RefreshPresentation(); }

ASovAurelionSupportPresentation::ASovAurelionSupportPresentation()
{
    PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.TickInterval=.1f;
    SceneRoot=CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")); SetRootComponent(SceneRoot);
    WestBarrierVisual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WestBarrierVisual")); WestBarrierVisual->SetupAttachment(SceneRoot);
    EastBarrierVisual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EastBarrierVisual")); EastBarrierVisual->SetupAttachment(SceneRoot);
    WestBarrierVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision); EastBarrierVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void ASovAurelionSupportPresentation::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!IsValid(Support) || Support->IsActorBeingDestroyed() || Support->GetWorld()!=GetWorld()) { return; }
    const auto Apply=[](UStaticMeshComponent* Mesh,UBoxComponent* Barrier)
    {
        if (!IsValid(Mesh) || !IsValid(Barrier) || !Barrier->IsRegistered()) { return; }
        Mesh->SetWorldLocationAndRotation(Barrier->GetComponentLocation(),Barrier->GetComponentQuat());
        Mesh->SetWorldScale3D(Barrier->GetScaledBoxExtent()/50.f);
        Mesh->SetVisibility(Barrier->GetCollisionEnabled()!=ECollisionEnabled::NoCollision);
    };
    Apply(WestBarrierVisual,Support->WestCacheBarrier); Apply(EastBarrierVisual,Support->EastFlankBarrier);
}
#undef LOCTEXT_NAMESPACE
