// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Corruption/SovCorruptionSourceVolume.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "Corruption/SovCorruptionMath.h"
#include "Corruption/SovCorruptionSourceComponent.h"
#include "Engine/World.h"
#include "UnrealFramework/NarrativeCharacter.h"

namespace
{
	AActor* CorruptionTarget(AActor* Actor)
	{
		if (!IsValid(Actor)) { return nullptr; }
		if (const auto* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor)) { return ASC->GetAvatarActor(); }
		if (const auto* Provider = Cast<INarrativeCharacterOwner>(Actor)) { return Provider->GetNarrativeCharacter(); }
		return Actor;
	}
}
ASovCorruptionSourceVolume::ASovCorruptionSourceVolume()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
	ExposureSphere = CreateDefaultSubobject<USphereComponent>(TEXT("ExposureSphere"));
	SetRootComponent(ExposureSphere);
	ExposureSphere->SetSphereRadius(500.0f);
	ExposureSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ExposureSphere->SetCollisionObjectType(ECC_WorldDynamic);
	ExposureSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	ExposureSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ExposureSphere->SetGenerateOverlapEvents(true);
	ExposureSphere->SetHiddenInGame(true);
}
void ASovCorruptionSourceVolume::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority()) { SetCorruptionProfile(Profile); }
}
bool ASovCorruptionSourceVolume::SetCorruptionProfile(USovCorruptionProfile* NewProfile)
{
	if (!HasAuthority() || IsActorBeingDestroyed()) { return false; }
	FString Error;
	if (NewProfile && (NewProfile->SourceKind != ESovCorruptionSourceKind::Environment || !NewProfile->ValidateProfile(Error))) { return false; }
	ReleaseContacts();
	Profile = NewProfile;
	if (Profile) { ExposureSphere->SetSphereRadius(Profile->Radius, true); }
	return true;
}
bool ASovCorruptionSourceVolume::ValidateContact(AActor* Target, float& OutFalloff) const
{
	OutFalloff = 0.0f;
	FString Error;
	if (!HasAuthority() || IsActorBeingDestroyed() || !IsValid(Profile) || !Profile->ValidateProfile(Error)
		|| Profile->SourceKind != ESovCorruptionSourceKind::Environment || !IsValid(Target) || Target == this || Target->IsActorBeingDestroyed() || !IsValid(ExposureSphere)
		|| !ExposureSphere->IsOverlappingActor(Target)) { return false; }
	if (Profile->Escape == ESovCorruptionEscape::DestroyNode && !IsValid(SourceNode)) { return false; }
	if (SourceNode)
	{
		const auto* Producer = SourceNode->FindComponentByClass<USovCorruptionSourceComponent>();
		if (!Producer || Producer->Profile != Profile || Producer->IsResolvedFor(Target)) { return false; }
	}
	const float Distance = FVector::Distance(GetActorLocation(), Target->GetActorLocation());
	OutFalloff = SovCorruptionMath::Falloff(Distance, Profile->Radius, Profile->bLinearFalloff);
	if (OutFalloff <= 0.0f) { return false; }
	if (Profile->bRequiresLineOfSight)
	{
		FCollisionQueryParams Query(SCENE_QUERY_STAT(CorruptionSourceLOS), false, this);
		Query.AddIgnoredActor(Target);
		TArray<AActor*> Attached;
		Target->GetAttachedActors(Attached, true, true);
		Query.AddIgnoredActors(Attached);
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, GetActorLocation(), Target->GetActorLocation(), ECC_Visibility, Query)
			&& CorruptionTarget(Hit.GetActor()) != Target) { return false; }
	}
	return true;
}
void ASovCorruptionSourceVolume::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority()) { RefreshContacts(); }
}
void ASovCorruptionSourceVolume::RefreshContacts()
{
	FString Error;
	if (!IsValid(Profile) || !Profile->ValidateProfile(Error)) { ReleaseContacts(); return; }
	if (!FMath::IsNearlyEqual(ExposureSphere->GetUnscaledSphereRadius(), Profile->Radius))
	{
		ExposureSphere->SetSphereRadius(Profile->Radius, true);
	}
	TArray<AActor*> Overlaps;
	ExposureSphere->GetOverlappingActors(Overlaps);
	TSet<USovCorruptionComponent*> Present;
	for (AActor* Actor : Overlaps)
	{
		AActor* Target = CorruptionTarget(Actor);
		float Falloff = 0.0f;
		if (!ValidateContact(Target, Falloff)) { continue; }
		auto* Component = Target->FindComponentByClass<USovCorruptionComponent>();
		if (!Component || Present.Contains(Component)) { continue; }
		Present.Add(Component);
		// Acquire also repairs a stale local handle after Narrative restores the target component.
		const FSovCorruptionSourceHandle Handle = Component->AcquireSource(this);
		if (IsActorBeingDestroyed()) { return; }
		if (Handle.IsValid()) { Contacts.Add(Component, Handle); }
	}
	for (auto It = Contacts.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || !Present.Contains(It.Key().Get()))
		{
			if (It.Key().IsValid()) { It.Key()->ReleaseSource(It.Value(), this); }
			It.RemoveCurrent();
		}
	}
}
void ASovCorruptionSourceVolume::ReleaseContacts()
{
	const auto Previous = MoveTemp(Contacts);
	Contacts.Empty();
	for (const auto& Pair : Previous)
	{
		if (Pair.Key.IsValid()) { Pair.Key->ReleaseSource(Pair.Value, this); }
	}
}
void ASovCorruptionSourceVolume::EndPlay(const EEndPlayReason::Type Reason)
{
	ReleaseContacts();
	Super::EndPlay(Reason);
}
