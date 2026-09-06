// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovEchoBypassGate.h"
#include "Campaign/SovEncounterDirector.h"
#include "Characters/SovNPCCharacterBase.h"
#include "Components/SovSeleneEchoGenerationComponent.h"
#include "Components/BoxComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "ArsenalStatics.h"
#include "Engine/World.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "NarrativeGameplayTags.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense_Sight.h"
#include "Sovereign/SovGameplayTags.h"

ASovEchoBypassGate::ASovEchoBypassGate()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	EntryVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("EntryVolume"));
	SetRootComponent(EntryVolume);
	ExitVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("ExitVolume"));
	ExitVolume->SetupAttachment(EntryVolume);
	ExitVolume->SetRelativeLocation(FVector(500.f, 0.f, 0.f));
	for (UBoxComponent* Volume : {EntryVolume.Get(), ExitVolume.Get()})
	{
		Volume->SetBoxExtent(FVector(75.f, 150.f, 150.f));
		Volume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Volume->SetCollisionResponseToAllChannels(ECR_Ignore);
		Volume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		Volume->SetGenerateOverlapEvents(true);
	}
}

void ASovEchoBypassGate::BeginPlay()
{
	Super::BeginPlay();
	SetActorTickEnabled(HasAuthority());
	if (!HasAuthority()) return;
	EntryVolume->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::HandleEntry);
	ExitVolume->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::HandleExit);
}

void ASovEchoBypassGate::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearCandidate();
	Super::EndPlay(EndPlayReason);
}

void ASovEchoBypassGate::HandleEntry(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*,
	int32, bool, const FHitResult&)
{
	if (!HasAuthority() || !IsValid(Encounter) || Encounter->GetEncounterState() != ESovEncounterState::Active
		|| !IsValid(OtherActor) || !OtherActor->FindComponentByClass<USovSeleneEchoGenerationComponent>()
		|| ThreatParticipantIds.IsEmpty() || !Encounter->GetAttemptId().IsValid()
		|| !EntryVolume->IsOverlappingActor(OtherActor) || ExitVolume->IsOverlappingActor(OtherActor)) return;
	// Repeated capsule/mesh overlap notifications must not erase detected traversal history.
	if (Candidate.Get() == OtherActor && CandidateAttempt == Encounter->GetAttemptId()) return;
	ClearCandidate();
	Candidate = OtherActor;
	CandidateAttempt = Encounter->GetAttemptId();
	EnteredAt = GetWorld()->GetTimeSeconds();
	for (FName Id : ThreatParticipantIds)
	{
		ASovNPCCharacterBase* Threat = Encounter->GetParticipant(Id);
		AAIController* Controller = Threat ? Cast<AAIController>(Threat->GetController()) : nullptr;
		UAIPerceptionComponent* Perception = Controller ? Controller->GetPerceptionComponent() : nullptr;
		UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Threat);
		if (!IsValid(Threat) || !Perception || !Perception->IsRegistered()
			|| !Perception->IsSenseEnabled(UAISense_Sight::StaticClass()) || !ASC
			|| ASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Status_DeviceDisabled))
		{
			ClearCandidate();
			return;
		}
		CandidateThreats.Add(Threat);
		Perceptions.Add(Perception);
		Perception->OnTargetPerceptionUpdated.AddUniqueDynamic(this, &ThisClass::HandlePerception);
	}
	bool bDisabled = false;
	if (!ValidateCandidate(bDisabled)) ClearCandidate();
}

bool ASovEchoBypassGate::ValidateCandidate(bool& bOutNewlyDisabled) const
{
	bOutNewlyDisabled = false;
	if (!HasAuthority() || !Candidate.IsValid() || !IsValid(Encounter)
		|| Encounter->GetEncounterState() != ESovEncounterState::Active
		|| Encounter->GetAttemptId() != CandidateAttempt || !FMath::IsFinite(MaximumTraversalSeconds)
		|| GetWorld()->GetTimeSeconds() - EnteredAt > FMath::Max(MaximumTraversalSeconds, 0.1f)
		|| CandidateThreats.Num() != ThreatParticipantIds.Num() || CandidateThreats.IsEmpty()) return false;
	UAbilitySystemComponent* PlayerASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Candidate.Get());
	const FSovGameplayTags& GameplayTags = FSovGameplayTags::Get();
	if (!PlayerASC || !PlayerASC->HasMatchingGameplayTag(GameplayTags.Character_Player_Selene)
		|| PlayerASC->HasMatchingGameplayTag(GameplayTags.Character_Player_Tarrik)
		|| PlayerASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_IsDead)
		|| PlayerASC->HasMatchingGameplayTag(GameplayTags.State_Fatal)) return false;
	for (int32 Index = 0; Index < CandidateThreats.Num(); ++Index)
	{
		ASovNPCCharacterBase* Threat = CandidateThreats[Index].Get();
		UAIPerceptionComponent* Perception = Perceptions[Index].Get();
		AAIController* Controller = Threat ? Cast<AAIController>(Threat->GetController()) : nullptr;
		UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Threat);
		if (!IsValid(Threat) || Encounter->GetParticipant(ThreatParticipantIds[Index]) != Threat
			|| !Controller || Controller->GetPerceptionComponent() != Perception || !Perception
			|| !Perception->IsRegistered() || !Perception->IsSenseEnabled(UAISense_Sight::StaticClass()) || !ASC
			|| !ASC->GetSet<UNarrativeAttributeSetBase>()
			|| ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) <= 0.f
			|| ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_IsDead)
			|| ASC->HasMatchingGameplayTag(GameplayTags.State_Fatal)
			|| UArsenalStatics::GetAttitude(Candidate.Get(), Threat) != ETeamAttitude::Hostile) return false;
		TArray<AActor*> KnownActors;
		Perception->GetKnownPerceivedActors(nullptr, KnownActors);
		if (KnownActors.Contains(Candidate.Get())) return false;
		bOutNewlyDisabled |= ASC->HasMatchingGameplayTag(GameplayTags.State_Status_DeviceDisabled);
	}
	return true;
}

void ASovEchoBypassGate::HandlePerception(AActor* Actor, FAIStimulus Stimulus)
{
	if (Actor == Candidate.Get() && Stimulus.WasSuccessfullySensed())
	{
		// Keep a consumed candidate until a new entry, so repeated body overlaps cannot restart it.
		bConsumed = true;
		PendingReceipt.Invalidate();
	}
}

void ASovEchoBypassGate::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Candidate.IsValid() || bConsumed) return;
	bool bDisabled = false;
	if (!ValidateCandidate(bDisabled)) { bConsumed = true; return; }
	if (bDisabled) IssueReceipt();
}

void ASovEchoBypassGate::HandleExit(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*,
	int32, bool, const FHitResult&)
{
	if (OtherActor == Candidate.Get() && !bConsumed && ExitVolume->IsOverlappingActor(OtherActor)
		&& !EntryVolume->IsOverlappingActor(OtherActor)) IssueReceipt();
}

void ASovEchoBypassGate::IssueReceipt()
{
	bool bDisabled = false;
	if (bConsumed || !ValidateCandidate(bDisabled)) return;
	USovSeleneEchoGenerationComponent* Generator = Candidate->FindComponentByClass<USovSeleneEchoGenerationComponent>();
	if (!Generator) return;
	PendingReceipt = FGuid::NewGuid();
	Generator->ConsumeUndetectedBypass(this, PendingReceipt);
	// Even rejected/full-meter receipts cannot be replayed after a later spend.
	bConsumed = true;
	PendingReceipt.Invalidate();
}

bool ASovEchoBypassGate::ConsumeReceipt(AActor* Player, const FGuid& ReceiptId, FGuid& OutAttemptId)
{
	OutAttemptId.Invalidate();
	bool bDisabled = false;
	if (!ReceiptId.IsValid() || ReceiptId != PendingReceipt || Player != Candidate.Get()
		|| bConsumed || !ValidateCandidate(bDisabled)) return false;
	bConsumed = true;
	PendingReceipt.Invalidate();
	if (!Encounter->ClaimAttemptReward(TEXT("Selene.UndetectedBypass"))) return false;
	OutAttemptId = CandidateAttempt;
	return true;
}

void ASovEchoBypassGate::ClearCandidate()
{
	for (const TWeakObjectPtr<UAIPerceptionComponent>& Perception : Perceptions)
		if (Perception.IsValid()) Perception->OnTargetPerceptionUpdated.RemoveDynamic(this, &ThisClass::HandlePerception);
	Candidate.Reset();
	CandidateThreats.Reset();
	Perceptions.Reset();
	CandidateAttempt.Invalidate();
	PendingReceipt.Invalidate();
	bConsumed = false;
}
