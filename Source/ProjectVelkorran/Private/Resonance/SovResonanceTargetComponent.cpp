// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Resonance/SovResonanceTargetComponent.h"
#include "Resonance/SovResonanceComponent.h"
#include "Resonance/SovResonancePolicy.h"
#include "Components/SovCommandLinkComponent.h"
#include "Components/SovWeakPointComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"

USovResonanceTargetComponent::USovResonanceTargetComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}
void USovResonanceTargetComponent::BeginPlay()
{
	Super::BeginPlay();
	WeakPoints = GetOwner()->FindComponentByClass<USovWeakPointComponent>();
	if (WeakPoints && GetOwner()->HasAuthority())
	{ WeakPoints->OnWeakPointRevealStateChanged.AddUniqueDynamic(this, &ThisClass::ObserveReveal); }
}
void USovResonanceTargetComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	EndTerminalOperation();
	if (IsValid(WeakPoints)) { WeakPoints->OnWeakPointRevealStateChanged.RemoveDynamic(this, &ThisClass::ObserveReveal); }
	Super::EndPlay(Reason);
}
void USovResonanceTargetComponent::ObserveReveal(bool bRevealed, float Remaining, AActor* Instigator)
{
	ExposedBy = bRevealed ? Instigator : nullptr;
	ExposureUntil = GetWorld()->GetTimeSeconds() + FMath::Max(0.f, Remaining);
	if (bRevealed)
	{ if (auto* Coordinator = USovResonanceComponent::FindActive(GetWorld())) { Coordinator->ObserveExposure(this, Instigator); } }
}
bool USovResonanceTargetComponent::IsExposedBy(AActor* Selene) const
{
	return IsValid(Selene) && ExposedBy == Selene && GetWorld() && GetWorld()->GetTimeSeconds() < ExposureUntil
		&& IsValid(WeakPoints) && WeakPoints->IsWeakPointRevealActive();
}
USovCommandLinkComponent* USovResonanceTargetComponent::GetSupportLink() const
{
	AActor* Source = IsValid(SupportLinkOwner) ? SupportLinkOwner.Get() : GetOwner();
	return IsValid(Source) ? Source->FindComponentByClass<USovCommandLinkComponent>() : nullptr;
}
bool USovResonanceTargetComponent::CanResolve(ESovResonanceType Type, const USovResonanceComponent* Coordinator) const
{
	if (!Coordinator || !IsValid(GetOwner()) || GetOwner()->IsActorBeingDestroyed() || !AllowedTypes.Contains(Type)
		|| !Coordinator->IsPermitted(Type, GetOwner())) { return false; }
	switch (Type)
	{
	case ESovResonanceType::SupportSever: return GetSupportLink() && GetSupportLink()->IsCommandLinkActive();
	case ESovResonanceType::FormationBreach: return IsExposedBy(Coordinator->GetSelene());
	case ESovResonanceType::TerminalRelease: return bTerminalCompleted && ProtectedPressure > 0.f;
	case ESovResonanceType::AdvanceCorridor: return IsWithinCorridor(Coordinator->GetTarrik()) && IsWithinCorridor(Coordinator->GetSelene());
	default: return false;
	}
}
bool USovResonanceTargetComponent::BeginTerminalOperation(AActor* Selene, FString& Reason)
{
	Reason.Reset();
	auto* Coordinator = USovResonanceComponent::FindActive(GetWorld());
	auto* ASC = IsValid(Selene) ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Selene) : nullptr;
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Coordinator || Coordinator->GetSelene() != Selene
		|| !Coordinator->IsPermitted(ESovResonanceType::TerminalRelease, GetOwner()) || !AllowedTypes.Contains(ESovResonanceType::TerminalRelease)
		|| !ASC || !ASC->GetSet<UNarrativeAttributeSetBase>() || ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) <= 0.f
		|| ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_SequencerControlled)
		|| ASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Fatal)
		|| Operator.IsValid() || bTerminalCompleted || !FMath::IsFinite(TerminalHoldSeconds) || TerminalHoldSeconds < .5f
		|| FVector::DistSquared(Selene->GetActorLocation(), GetOwner()->GetActorLocation()) > FMath::Square(InteractionRange))
	{ Reason = TEXT("Selene cannot begin this permitted terminal action from the current state or position."); return false; }
	Operator = Selene; OperationStarted = GetWorld()->GetTimeSeconds(); ProtectedPressure = 0.f;
	SetComponentTickEnabled(true); return true;
}
void USovResonanceTargetComponent::EndTerminalOperation()
{
	Operator.Reset(); ProtectedPressure = 0.f; SetComponentTickEnabled(false);
}
void USovResonanceTargetComponent::RecordProtection(AActor* Selene, float Pressure)
{
	if (Operator == Selene && !bTerminalCompleted)
	{ ProtectedPressure = SovResonancePolicy::AddPressure(ProtectedPressure, Pressure, FMath::Clamp(MaximumPressure, 1.f, 500.f)); }
}
void USovResonanceTargetComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Function)
{
	Super::TickComponent(DeltaTime, TickType, Function);
	auto* Coordinator = USovResonanceComponent::FindActive(GetWorld());
	auto* ASC = Operator.IsValid() ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Operator.Get()) : nullptr;
	if (!GetOwner()->HasAuthority() || !Coordinator || Coordinator->GetSelene() != Operator.Get()
		|| !Coordinator->IsPermitted(ESovResonanceType::TerminalRelease, GetOwner()) || !ASC
		|| ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) <= 0.f
		|| ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_IsDead)
		|| ASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Fatal)
		|| ASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Poise_Broken)
		|| FVector::DistSquared(Operator->GetActorLocation(), GetOwner()->GetActorLocation()) > FMath::Square(InteractionRange))
	{ EndTerminalOperation(); return; }
	if (GetWorld()->GetTimeSeconds() - OperationStarted >= FMath::Clamp(TerminalHoldSeconds, .5f, 15.f))
	{
		bTerminalCompleted = true; Operator.Reset(); SetComponentTickEnabled(false);
		if (ProtectedPressure > 0.f) { Coordinator->MakeOffer(ESovResonanceType::TerminalRelease, this, ProtectedPressure); }
	}
}

bool USovResonanceTargetComponent::IsWithinCorridor(const AActor* Actor) const
{
	if (!IsValid(CorridorAnchor) || !IsValid(Actor) || CorridorAnchor->GetWorld() != GetWorld()) { return false; }
	const FVector Local = CorridorAnchor->GetActorTransform().InverseTransformPositionNoScale(Actor->GetActorLocation());
	return !Local.ContainsNaN() && FMath::Abs(Local.X) <= FMath::Clamp(CorridorLength, 100.f, 2500.f) * .5f
		&& FMath::Abs(Local.Y) <= FMath::Clamp(CorridorHalfWidth, 50.f, 500.f) && FMath::Abs(Local.Z) <= 150.f;
}
void USovResonanceTargetComponent::Load_Implementation()
{
	EndTerminalOperation(); ExposedBy.Reset(); ExposureUntil = 0.f;
}
