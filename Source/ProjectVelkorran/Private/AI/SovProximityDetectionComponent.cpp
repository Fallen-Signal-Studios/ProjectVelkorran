// Copyright Fallen Signal Studios. All Rights Reserved.

#include "AI/SovProximityDetectionComponent.h"

#include "AI/SovProximityDetectionPolicy.h"
#include "Combat/SovTarrikPayloadSupport.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"

USovProximityDetectionComponent::USovProximityDetectionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	// A sweep, not a per-frame scan: the radar does not need to resolve faster than a player reads it.
	PrimaryComponentTick.TickInterval = .1f;
	SetIsReplicatedByDefault(false);
}

int32 USovProximityDetectionComponent::GetLiveSightingCount() const
{
	int32 Live = 0;
	for (const FSovProximityContact& Contact : Contacts) { if (Contact.bLiveSighting) { ++Live; } }
	return Live;
}

float USovProximityDetectionComponent::FacingYawDegrees() const
{
	const AActor* const Owner = GetOwner();
	if (!IsValid(Owner)) { return 0.f; }
	// The player's view decides what "ahead" means on the display, not the pawn's body.
	if (const auto* Pawn = Cast<APawn>(Owner))
	{
		if (const AController* const Controller = Pawn->GetController())
		{
			return Controller->GetControlRotation().Yaw;
		}
	}
	return Owner->GetActorRotation().Yaw;
}

bool USovProximityDetectionComponent::IsHostileTarget(AActor* Target) const
{
	AActor* const Owner = GetOwner();
	if (!IsValid(Owner) || !IsValid(Target) || Target == Owner) { return false; }
	// Hostility is the owner's own team judgement, so allies and summoned friendlies never register.
	const auto* Team = Cast<INarrativeTeamAgentInterface>(Owner);
	if (!Team || Team->GetTeamAttitudeTowards(*Target) != ETeamAttitude::Hostile) { return false; }
	return SovTarrikPayload::Alive(SovTarrikPayload::ResolveASC(Target));
}

bool USovProximityDetectionComponent::HasLineOfSight(const AActor* Target) const
{
	const AActor* const Owner = GetOwner();
	UWorld* const World = GetWorld();
	if (!World || !IsValid(Owner) || !IsValid(Target)) { return false; }
	FVector Origin = Owner->GetActorLocation();
	if (const auto* Pawn = Cast<APawn>(Owner)) { Origin = Pawn->GetPawnViewLocation(); }
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SovProximityDetection), false);
	SovTarrikPayload::IgnoreActorAndAttachments(Query, const_cast<AActor*>(Owner));
	SovTarrikPayload::IgnoreActorAndAttachments(Query, const_cast<AActor*>(Target));
	FHitResult Hit;
	// Anything solid between the two hides it: the radar never reports through geometry.
	return !World->LineTraceSingleByChannel(Hit, Origin, Target->GetActorLocation(), ECC_Visibility, Query);
}

void USovProximityDetectionComponent::Observe(float DeltaSeconds)
{
	UWorld* const World = GetWorld();
	AActor* const Owner = GetOwner();
	Contacts.Reset();
	if (!World || !IsValid(Owner)) { Tracked.Reset(); return; }
	const float Delta = FMath::IsFinite(DeltaSeconds) && DeltaSeconds > 0.f ? DeltaSeconds : 0.f;
	const FVector Origin = Owner->GetActorLocation();

	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams Objects; Objects.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SovProximitySweep), false, Owner);
	World->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, Objects,
		FCollisionShape::MakeSphere(SovProximityDetectionPolicy::DetectionRadius), Query);

	// Everything seen this sweep, so tracked entries that were not seen can age.
	TSet<AActor*> SeenThisSweep;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* const Candidate = Overlap.GetActor();
		if (!IsHostileTarget(Candidate) || SeenThisSweep.Contains(Candidate)) { continue; }
		const float Distance = FVector::Distance(Origin, Candidate->GetActorLocation());
		const bool bVisible = SovProximityDetectionPolicy::AdmitsNewContact(HasLineOfSight(Candidate), Distance);
		if (!bVisible) { continue; }
		SeenThisSweep.Add(Candidate);

		FTracked* Entry = Tracked.FindByPredicate([Candidate](const FTracked& Row) { return Row.Actor.Get() == Candidate; });
		if (!Entry) { Entry = &Tracked.AddDefaulted_GetRef(); Entry->Actor = Candidate; }
		Entry->ContinuouslyVisibleSeconds += Delta;
		Entry->SecondsSinceSeen = 0.f;
		Entry->LastSeenLocation = Candidate->GetActorLocation();
		// A glimpse does not register; once acquired, it stays acquired for as long as it is remembered.
		Entry->bAcquired = Entry->bAcquired || SovProximityDetectionPolicy::HasAcquired(Entry->ContinuouslyVisibleSeconds);
	}

	const float Facing = FacingYawDegrees();
	for (int32 Index = Tracked.Num() - 1; Index >= 0; --Index)
	{
		FTracked& Entry = Tracked[Index];
		AActor* const Actor = Entry.Actor.Get();
		if (!IsValid(Actor) || !SovTarrikPayload::Alive(SovTarrikPayload::ResolveASC(Actor)))
		{
			Tracked.RemoveAtSwap(Index);
			continue;
		}
		if (!SeenThisSweep.Contains(Actor))
		{
			// Out of sight and out of range are the same thing here: it fades rather than blinking out.
			Entry.ContinuouslyVisibleSeconds = 0.f;
			Entry.SecondsSinceSeen += Delta;
		}
		if (!SovProximityDetectionPolicy::ShouldRetain(Entry.SecondsSinceSeen))
		{
			Tracked.RemoveAtSwap(Index);
			continue;
		}
		if (!Entry.bAcquired) { continue; }

		const FVector ToTarget = Entry.LastSeenLocation - Origin;
		FSovProximityContact Contact;
		Contact.Actor = Entry.Actor;
		Contact.LastSeenLocation = Entry.LastSeenLocation;
		Contact.BearingDegrees = SovProximityDetectionPolicy::RelativeBearingDegrees(Facing, ToTarget.Rotation().Yaw);
		Contact.NormalisedRange = SovProximityDetectionPolicy::NormalisedRange(ToTarget.Size2D());
		Contact.Alpha = SovProximityDetectionPolicy::ContactAlpha(Entry.SecondsSinceSeen);
		Contact.bLiveSighting = SovProximityDetectionPolicy::IsLiveSighting(Entry.SecondsSinceSeen);
		Contacts.Add(Contact);
	}
	// Live sightings first, then the freshest memories: the display draws the most certain on top.
	Contacts.Sort([](const FSovProximityContact& A, const FSovProximityContact& B) { return A.Alpha > B.Alpha; });
}

void USovProximityDetectionComponent::TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaSeconds, TickType, TickFunction);
	Observe(DeltaSeconds);
}
