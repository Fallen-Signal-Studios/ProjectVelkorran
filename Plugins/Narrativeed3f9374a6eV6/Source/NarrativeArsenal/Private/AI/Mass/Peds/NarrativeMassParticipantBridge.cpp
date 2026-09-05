// Copyright Fallen Signal Studios. All Rights Reserved.
#include "AI/Mass/Peds/NarrativeMassParticipantBridge.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"

UNarrativeMassParticipantReceiptComponent::UNarrativeMassParticipantReceiptComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UNarrativeMassParticipantReceiptComponent::Bind(TSharedRef<FMassEntityManager> InEntityManager,
	const FMassEntityHandle InEntity)
{
	Reset();
	if (bBound) { return false; } // A release callback established a newer association.
	AActor* Actor = GetOwner();
	if (!IsValid(Actor) || Actor->IsActorBeingDestroyed() || !InEntityManager->IsEntityValid(InEntity)) { return false; }
	const auto* Fragment = InEntityManager->GetFragmentDataPtr<FNarrativeMassParticipantFragment>(InEntity);
	if (!Fragment || !Fragment->HasValidIdentity()) { return false; }
	const FNarrativeMassParticipantFragment NewParticipant = *Fragment;
	UObject* Owner = NewParticipant.Owner.Get();
	const AActor* OwnerActor = Cast<AActor>(Owner);
	auto* Policy = Cast<INarrativeMassParticipantOwner>(Owner);
	UWorld* World = Actor->GetWorld();
	UMassEntitySubsystem* Subsystem = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (!Policy || (OwnerActor && OwnerActor->IsActorBeingDestroyed()) || !World || Owner->GetWorld() != World || !Subsystem
		|| &Subsystem->GetMutableEntityManager() != &InEntityManager.Get()) { return false; }

	Participant = NewParticipant;
	Entity = InEntity;
	EntityManager = InEntityManager;
	bBound = true;
	const uint64 ExpectedEpoch = ++BindingEpoch;
	const bool bAccepted = Policy->AcceptMassRepresentation(InEntityManager.Get(), InEntity, *Actor, NewParticipant);
	// The callback is project code and can synchronously invalidate or rebind this actor.
	if (!bAccepted || !MatchesReceipt(InEntityManager.Get(), ExpectedEpoch))
	{
		if (BindingEpoch == ExpectedEpoch) { Reset(); }
		return false;
	}
	return true;
}

bool UNarrativeMassParticipantReceiptComponent::IsCurrent(FMassEntityManager& InEntityManager) const
{
	const TSharedPtr<FMassEntityManager> BoundManager = EntityManager.Pin();
	AActor* Actor = GetOwner();
	UObject* Owner = Participant.Owner.Get();
	const AActor* OwnerActor = Cast<AActor>(Owner);
	if (!bBound || !BoundManager.IsValid() || BoundManager.Get() != &InEntityManager
		|| !IsValid(Actor) || Actor->IsActorBeingDestroyed() || !IsValid(Owner)
		|| (OwnerActor && OwnerActor->IsActorBeingDestroyed())
		|| !Actor->GetWorld() || Owner->GetWorld() != Actor->GetWorld()
		|| !InEntityManager.IsEntityValid(Entity)) { return false; }
	const auto* Fragment = InEntityManager.GetFragmentDataPtr<FNarrativeMassParticipantFragment>(Entity);
	const auto* Policy = Cast<INarrativeMassParticipantOwner>(Owner);
	if (!Fragment || !Fragment->HasValidIdentity() || !Fragment->HasSameIdentity(Participant) || !Policy) { return false; }
	const uint64 ExpectedEpoch = BindingEpoch;
	const FNarrativeMassParticipantFragment ExpectedParticipant = Participant;
	const bool bCurrent = Policy->IsMassRepresentationCurrent(InEntityManager, Entity, *Actor, ExpectedParticipant);
	return bCurrent && bBound && BindingEpoch == ExpectedEpoch;
}

bool UNarrativeMassParticipantReceiptComponent::MatchesReceipt(FMassEntityManager& InEntityManager,
	const uint64 ExpectedEpoch) const
{
	return BindingEpoch == ExpectedEpoch && IsCurrent(InEntityManager) && BindingEpoch == ExpectedEpoch;
}

void UNarrativeMassParticipantReceiptComponent::Reset()
{
	const bool bWasBound = bBound;
	const FNarrativeMassParticipantFragment OldParticipant = Participant;
	const FMassEntityHandle OldEntity = Entity;
	const TSharedPtr<FMassEntityManager> OldManager = EntityManager.Pin();
	// Invalidate before notifying external code, which may re-enter the bridge.
	bBound = false;
	++BindingEpoch;
	Participant = FNarrativeMassParticipantFragment();
	Entity = FMassEntityHandle();
	EntityManager.Reset();
	AActor* Actor = GetOwner();
	UObject* Owner = OldParticipant.Owner.Get();
	if (bWasBound && OldManager.IsValid() && IsValid(Owner) && Actor
		&& Owner->GetWorld() == Actor->GetWorld())
	{
		if (auto* Policy = Cast<INarrativeMassParticipantOwner>(Owner))
		{
			Policy->ReleaseMassRepresentation(*OldManager, OldEntity, *Actor, OldParticipant);
		}
	}
}

void UNarrativeMassParticipantReceiptComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Reset();
	Super::EndPlay(EndPlayReason);
}

void UNarrativeMassParticipantReceiptComponent::OnComponentDestroyed(const bool bDestroyingHierarchy)
{
	Reset();
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}
