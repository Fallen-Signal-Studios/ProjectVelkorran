// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Components/SovCorruptionComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Corruption/SovCorruptionMath.h"
#include "Corruption/SovCorruptionSourceVolume.h"
#include "Effects/SovGameplayEffect_CorruptionBand.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "NarrativeGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "Sovereign/SovGameplayTags.h"

USovCorruptionComponent::USovCorruptionComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
}
void USovCorruptionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Props) const
{
	Super::GetLifetimeReplicatedProps(Props);
	DOREPLIFETIME(USovCorruptionComponent, State);
}
bool USovCorruptionComponent::ValidOwner() const
{
	AActor* Owner = GetOwner();
	const auto* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Owner);
	return !bEnding && IsValid(Owner) && Owner->HasAuthority() && !Owner->IsActorBeingDestroyed()
		&& IsValid(ASC) && ASC->GetAvatarActor() == Owner
		&& !ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_IsDead)
		&& !ASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Fatal)
		&& (!ASC->GetSet<UNarrativeAttributeSetBase>()
			|| ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) > 0.0f);
}
bool USovCorruptionComponent::ValidBandTuning() const
{
	return SovCorruptionMath::ValidThresholds(TraceThreshold, IntrusionThreshold, ContestThreshold, OverwriteThreshold, BandHysteresis);
}
USovCampaignStateComponent* USovCorruptionComponent::Campaign() const
{
	const auto* Pawn = Cast<APawn>(GetOwner());
	return Pawn && Pawn->GetController() ? Pawn->GetController()->FindComponentByClass<USovCampaignStateComponent>() : nullptr;
}
bool USovCorruptionComponent::ValidateSourcePermission(ASovCorruptionSourceVolume* Source,
	ESovCorruptionBand& OutCap, FName& OutMission) const
{
	OutCap = ESovCorruptionBand::Clear; OutMission = NAME_None;
	if (bWaitingForMissionRestore || !ValidOwner() || !ValidBandTuning() || !IsValid(Source) || Source->GetWorld() != GetWorld()) { return false; }
	const USovCorruptionProfile* Profile = Source->GetCorruptionProfile();
	const auto* StateComponent = Campaign();
	const auto* Mission = StateComponent && StateComponent->IsStateValid() ? StateComponent->GetActiveMission() : nullptr;
	if (!IsValid(Profile) || !IsValid(Mission) || !Profile->PermissionForMission(Mission->MissionId, OutCap)) { return false; }
	if (!Profile->EscapeBeatId.IsNone() && (!Mission->FindBeat(Profile->EscapeBeatId)
		|| StateComponent->IsBeatComplete(Mission->MissionId, Profile->EscapeBeatId))) { return false; }
	const auto* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
	FGameplayTagContainer Owned;
	ASC->GetOwnedGameplayTags(Owned);
	bool bAllowed = false;
	for (const auto& Identity : Profile->AllowedProtagonists) { bAllowed |= Owned.HasTagExact(Identity); }
	if (!bAllowed || !Owned.HasTagExact(Mission->Protagonist)) { return false; }
	float Falloff;
	if (!Source->ValidateContact(GetOwner(), Falloff)) { return false; }
	if (Profile->bCanonPersistent)
	{
		const FSovCampaignBeatDefinition* Beat = Mission->FindBeat(Profile->ConsequenceBeatId);
		if (!Beat || Beat->bInteractiveChoice || !Beat->CinematicId.IsNone()) { return false; }
	}
	OutMission = Mission->MissionId;
	return true;
}
float USovCorruptionComponent::ExposureCap(ESovCorruptionBand Cap) const
{
	switch (Cap)
	{
	case ESovCorruptionBand::Trace: return std::nextafter(IntrusionThreshold, 0.0f);
	case ESovCorruptionBand::Intrusion: return std::nextafter(ContestThreshold, 0.0f);
	case ESovCorruptionBand::Contest: return std::nextafter(OverwriteThreshold, 0.0f);
	case ESovCorruptionBand::OverwriteRisk: return 100.0f;
	default: return 0.0f;
	}
}
float USovCorruptionComponent::TotalExposure() const
{
	float Total = 0.0f;
	for (const auto& Record : Records) { Total += Record.Exposure; }
	return FMath::Clamp(Total, 0.0f, 100.0f);
}
void USovCorruptionComponent::Accumulate(USovCorruptionProfile* Profile, FName MissionId,
	float Amount, ESovCorruptionBand Cap)
{
	const float Added = SovCorruptionMath::AddCapped(TotalExposure(), Amount, ExposureCap(Cap));
	if (Added <= 0.0f) { return; }
	for (auto& Record : Records)
	{
		if (Record.Profile == Profile && Record.MissionId == MissionId) { Record.Exposure += Added; return; }
	}
	FSovCorruptionExposureRecord Record;
	Record.Profile = Profile; Record.MissionId = MissionId; Record.Exposure = Added;
	Records.Add(Record);
}
FSovCorruptionSourceHandle USovCorruptionComponent::AcquireSource(ASovCorruptionSourceVolume* Source)
{
	FSovCorruptionSourceHandle Handle;
	ESovCorruptionBand Cap;
	FName MissionId;
	if (bMutating || !ValidateSourcePermission(Source, Cap, MissionId)) { return Handle; }
	USovCorruptionProfile* Profile = Source->GetCorruptionProfile();
	bool bSameProfileContact = false;
	for (const auto& Pair : Sources)
	{
		const auto* ExistingProfile = Pair.Value.Profile.Get();
		if (IsValid(ExistingProfile) && ExistingProfile != Profile && ExistingProfile->SourceId == Profile->SourceId)
		{
			return Handle;
		}
		if (Pair.Value.Source.Get() == Source && Pair.Value.Profile.Get() == Profile && Pair.Value.MissionId == MissionId)
		{
			Handle.Id = Pair.Key; return Handle;
		}
		bSameProfileContact |= Pair.Value.Profile.Get() == Profile;
	}
	for (const auto& Record : Records)
	{
		if (IsValid(Record.Profile) && Record.Profile != Profile && Record.Profile->SourceId == Profile->SourceId) { return Handle; }
	}
	TGuardValue<bool> MutationGuard(bMutating, true);
	Records.RemoveAll([MissionId](const auto& Record) { return Record.MissionId != MissionId; });
	Handle.Id = FGuid::NewGuid();
	FSourceEntry Entry; Entry.Source = Source; Entry.Profile = Profile; Entry.MissionId = MissionId;
	Sources.Add(Handle.Id, Entry);
	const bool bRestoredContact = RestoredContactProfiles.Remove(Profile->SourceId) > 0;
	if (!bSameProfileContact && !bRestoredContact)
	{
		float Falloff;
		if (Source->ValidateContact(GetOwner(), Falloff)) { Accumulate(Profile, MissionId, Profile->ContactExposure * Falloff, Cap); }
	}
	RefreshState(true);
	if (bEnding || !IsValid(Source) || Source->IsActorBeingDestroyed()) { Sources.Remove(Handle.Id); Handle.Id.Invalidate(); }
	return Handle;
}
void USovCorruptionComponent::ReleaseSource(FSovCorruptionSourceHandle Handle, ASovCorruptionSourceVolume* Source)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) { return; }
	const auto* Entry = Sources.Find(Handle.Id);
	if (Entry && Entry->Source.Get() == Source) { Sources.Remove(Handle.Id); }
}
void USovCorruptionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	if (bEnding || bMutating || !GetOwner() || !GetOwner()->HasAuthority()) { return; }
	if (bWaitingForMissionRestore)
	{
		// Legacy / encounter restores can publish readiness outside the campaign controller.
		// Managed handoffs call the explicit barrier before readiness and never consume this fallback.
		const auto* Player = Cast<ASovPlayerCharacterBase>(GetOwner());
		const auto* StateComponent = Campaign();
		if (Player && Player->IsCharacterReady() && StateComponent)
		{
			FString Error;
			FinishCampaignRestore(StateComponent->GetActiveMission(), Error);
		}
		return;
	}
	TGuardValue<bool> MutationGuard(bMutating, true);
	if (!ValidOwner() || !ValidBandTuning()) { Sources.Empty(); RefreshState(); return; }
	const auto* StateComponent = Campaign();
	const auto* Mission = StateComponent && StateComponent->IsStateValid() ? StateComponent->GetActiveMission() : nullptr;
	if (!Mission) { Sources.Empty(); RefreshState(); return; }
	const FName MissionId = Mission->MissionId;
	PruneRestoredContacts();
	Records.RemoveAll([MissionId](const auto& Record)
	{
		ESovCorruptionBand Cap;
		return !IsValid(Record.Profile) || Record.MissionId != MissionId
			|| !Record.Profile->PermissionForMission(MissionId, Cap) || !FMath::IsFinite(Record.Exposure) || Record.Exposure <= 0.0f;
	});
	const float Elapsed = FMath::IsFinite(DeltaTime) && DeltaTime > 0.0f ? DeltaTime : 0.0f;
	TSet<USovCorruptionProfile*> ContactProfiles;
	for (auto It = Sources.CreateIterator(); It; ++It)
	{
		ESovCorruptionBand Cap;
		FName SourceMission;
		auto* Source = It.Value().Source.Get();
		if (!ValidateSourcePermission(Source, Cap, SourceMission) || It.Value().MissionId != SourceMission
			|| It.Value().Profile.Get() != Source->GetCorruptionProfile()) { It.RemoveCurrent(); continue; }
		auto* Profile = Source->GetCorruptionProfile();
		ContactProfiles.Add(Profile);
		float Falloff;
		if (Source->ValidateContact(GetOwner(), Falloff)) { Accumulate(Profile, MissionId, Profile->ExposurePerSecond * Elapsed * Falloff, Cap); }
	}
	for (auto& Record : Records)
	{
		if (!ContactProfiles.Contains(Record.Profile.Get()) && Record.Profile->Escape == ESovCorruptionEscape::LeaveField)
		{
			Record.Exposure = FMath::Max(0.0f, Record.Exposure - Record.Profile->EscapeRecoveryPerSecond * Elapsed);
		}
	}
	Records.RemoveAll([](const auto& Record) { return Record.Exposure <= 0.0f; });
	RefreshState(true);
}
bool USovCorruptionComponent::CleanseExposure(float Amount, FName SourceId)
{
	if (bWaitingForMissionRestore || bMutating || bEnding || !GetOwner() || !GetOwner()->HasAuthority() || !FMath::IsFinite(Amount) || Amount <= 0.0f) { return false; }
	TGuardValue<bool> MutationGuard(bMutating, true);
	float Remaining = Amount;
	for (auto& Record : Records)
	{
		if (IsValid(Record.Profile) && (SourceId.IsNone() || Record.Profile->SourceId == SourceId))
		{
			const float Removed = FMath::Min(Record.Exposure, Remaining);
			Record.Exposure -= Removed; Remaining -= Removed;
		}
	}
	Records.RemoveAll([](const auto& Record) { return Record.Exposure <= 0.0f; });
	RefreshState(); // Campaign journal and state values are deliberately outside this operation.
	return Remaining < Amount;
}
void USovCorruptionComponent::RemoveOwnedBandEffect()
{
	const auto PreviousHandle = BandEffect;
	auto* PreviousASC = EffectASC.Get();
	BandEffect.Invalidate(); EffectASC.Reset(); AppliedEffectBand = ESovCorruptionBand::Clear;
	if (IsValid(PreviousASC) && PreviousHandle.IsValid()) { PreviousASC->RemoveActiveGameplayEffect(PreviousHandle); }
}
void USovCorruptionComponent::RefreshState(bool bCommitConsequences)
{
	if (bWaitingForMissionRestore) { return; }
	FSovCorruptionReplicatedState Next;
	const auto* StateComponent = Campaign();
	const auto* Mission = StateComponent && StateComponent->IsStateValid() ? StateComponent->GetActiveMission() : nullptr;
	ESovCorruptionBand AllowedCap = ESovCorruptionBand::Clear;
	if (ValidOwner() && ValidBandTuning() && IsValid(Mission))
	{
		Records.RemoveAll([Mission, StateComponent](const auto& Record)
		{
			return IsValid(Record.Profile) && !Record.Profile->EscapeBeatId.IsNone()
				&& (!Mission->FindBeat(Record.Profile->EscapeBeatId)
					|| StateComponent->IsBeatComplete(Mission->MissionId, Record.Profile->EscapeBeatId));
		});
		for (const auto& Record : Records)
		{
			ESovCorruptionBand Cap;
			if (IsValid(Record.Profile) && Record.MissionId == Mission->MissionId && Record.Exposure > 0.0f
				&& Record.Profile->PermissionForMission(Record.MissionId, Cap))
			{
				Next.Exposure += Record.Exposure;
				Next.Profiles.AddUnique(Record.Profile);
				AllowedCap = static_cast<ESovCorruptionBand>(FMath::Max(static_cast<uint8>(AllowedCap), static_cast<uint8>(Cap)));
			}
		}
		Next.Exposure = FMath::Clamp(Next.Exposure, 0.0f, ExposureCap(AllowedCap));
		const auto PreviousBand = bForceBandEffectRefresh ? RestoreBandSeed : State.Band;
		Next.Band = static_cast<ESovCorruptionBand>(FMath::Min(static_cast<int32>(AllowedCap), SovCorruptionMath::Band(
			Next.Exposure, static_cast<int32>(PreviousBand), TraceThreshold, IntrusionThreshold, ContestThreshold, OverwriteThreshold, BandHysteresis)));
	}
	const FSovCorruptionReplicatedState Previous = State;
	State = Next;
	// Publish complete SaveGame fields before GAS/tag/Blueprint notifications can request a checkpoint.
	RefreshSavedSnapshot();
	if (bCommitConsequences && !bEnding)
	{
		CommitAuthoredConsequences();
		if (bEnding) { return; }
		const auto* CurrentCampaign = Campaign();
		const bool bEscapeCommitted = CurrentCampaign && Records.ContainsByPredicate([Mission, CurrentCampaign](const auto& Record)
		{
			return IsValid(Record.Profile) && !Record.Profile->EscapeBeatId.IsNone()
				&& CurrentCampaign->IsBeatComplete(Mission->MissionId, Record.Profile->EscapeBeatId);
		});
		if (!ValidOwner() || !CurrentCampaign || CurrentCampaign->GetActiveMission() != Mission || bEscapeCommitted)
		{
			RefreshState(false); return;
		}
	}
	auto* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
	const bool bBandEffectMissing = !BandEffect.IsValid() || !EffectASC.IsValid()
		|| !EffectASC->GetActiveGameplayEffect(BandEffect);
	if (bForceBandEffectRefresh || AppliedEffectBand != State.Band || EffectASC.Get() != ASC || (bBandEffectMissing && State.Band != ESovCorruptionBand::Clear))
	{
		bForceBandEffectRefresh = false;
		RemoveOwnedBandEffect();
		if (bEnding) { return; }
		if (IsValid(ASC) && ValidOwner() && ASC->GetAvatarActor() == GetOwner()
			&& UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()) == ASC
			&& State.Band != ESovCorruptionBand::Clear)
		{
			const auto& Tags = FSovGameplayTags::Get();
			const FGameplayTag BandTags[] = {FGameplayTag(), Tags.State_Corruption_Trace, Tags.State_Corruption_Intrusion,
				Tags.State_Corruption_Contest, Tags.State_Corruption_OverwriteRisk};
			auto Spec = ASC->MakeOutgoingSpec(USovGameplayEffect_CorruptionBand::StaticClass(), 1.0f, ASC->MakeEffectContext());
			if (Spec.IsValid())
			{
				Spec.Data->DynamicGrantedTags.AddTag(BandTags[static_cast<uint8>(State.Band)]);
				const auto Applied = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
				if (bEnding || !ValidOwner() || UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()) != ASC)
				{
					if (IsValid(ASC) && Applied.IsValid()) { ASC->RemoveActiveGameplayEffect(Applied); }
					return;
				}
				BandEffect = Applied; EffectASC = ASC; AppliedEffectBand = State.Band;
			}
		}
	}
	if (!bEnding) { OnRep_State(Previous); }
}
void USovCorruptionComponent::CommitAuthoredConsequences()
{
	auto* StateComponent = Campaign();
	auto* Mission = StateComponent && StateComponent->IsStateValid() ? StateComponent->GetActiveMission() : nullptr;
	if (!Mission || !ValidOwner()) { return; }
	const auto Candidates = Records;
	for (const auto& Record : Candidates)
	{
		if (bEnding || !ValidOwner() || Campaign() != StateComponent || StateComponent->GetActiveMission() != Mission) { return; }
		const auto* Profile = Record.Profile.Get();
		if (!IsValid(Profile) || !Profile->bCanonPersistent || Record.MissionId != Mission->MissionId
			|| Record.Exposure <= 0.0f || State.Band < Profile->ConsequenceBand) { continue; }
		ESovCorruptionBand ProfileCap;
		if (!Profile->PermissionForMission(Mission->MissionId, ProfileCap) || ProfileCap < Profile->ConsequenceBand
			|| SovCorruptionMath::Band(Record.Exposure, 0, TraceThreshold, IntrusionThreshold,
				ContestThreshold, OverwriteThreshold, 0.0) < static_cast<int32>(Profile->ConsequenceBand)) { continue; }
		const auto* Beat = Mission->FindBeat(Profile->ConsequenceBeatId);
		if (Beat && !Beat->bInteractiveChoice && Beat->CinematicId.IsNone())
		{
			// Existing prerequisites, protected facts and idempotent journal transaction stay authoritative.
			StateComponent->CompleteBeat(Profile->ConsequenceBeatId, false);
		}
	}
}
FSovCorruptionPresentationRequest USovCorruptionComponent::GetPresentationRequest() const
{
	FSovCorruptionPresentationRequest Request;
	Request.Band = State.Band; Request.Exposure = State.Exposure; Request.bReducedEffects = bReducedEffects;
	for (const auto& Profile : State.Profiles)
	{
		if (!IsValid(Profile)) { continue; }
		Request.ProfileIds.Add(Profile->PresentationProfileId);
		Request.RemedyTexts.Add(Profile->RemedyText);
		Request.InformationTexts.Add(bReducedEffects ? Profile->ReducedEffectsSubstitute : Profile->InformationText);
		if (!bReducedEffects) { Request.SuggestedIntensity = FMath::Max(Request.SuggestedIntensity, Profile->PresentationIntensity); }
	}
	return Request;
}
void USovCorruptionComponent::SetReducedEffects(bool bEnabled)
{
	if (bReducedEffects == bEnabled) { return; }
	bReducedEffects = bEnabled;
	if (!bEnding) { OnPresentationRequested.Broadcast(GetPresentationRequest()); }
}
void USovCorruptionComponent::OnRep_State(FSovCorruptionReplicatedState Previous)
{
	if (Previous.Band != State.Band) { OnBandChanged.Broadcast(Previous.Band, State.Band); }
	if (!bEnding) { OnPresentationRequested.Broadcast(GetPresentationRequest()); }
}
void USovCorruptionComponent::PrepareForSave_Implementation()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bMutating || bEnding) { return; }
	RefreshSavedSnapshot();
}
void USovCorruptionComponent::RefreshSavedSnapshot()
{
	SavedSchemaVersion = 1; SavedBand = bWaitingForMissionRestore ? RestoreBandSeed : State.Band; SavedRecords.Empty();
	for (const auto& Record : Records)
	{
		if (!IsValid(Record.Profile) || !Record.Profile->bPersistExposureAtCheckpoint || Record.Exposure <= 0.0f) { continue; }
		auto Copy = Record;
		Copy.bContactAtCheckpoint = (bWaitingForMissionRestore && Record.bContactAtCheckpoint)
			|| RestoredContactProfiles.Contains(Record.Profile->SourceId);
		for (const auto& Pair : Sources) { Copy.bContactAtCheckpoint |= Pair.Value.Profile.Get() == Record.Profile.Get(); }
		SavedRecords.Add(Copy);
	}
}
void USovCorruptionComponent::PruneRestoredContacts()
{
	if (RestoredContactProfiles.IsEmpty() || !IsValid(GetOwner())) { return; }
	TArray<AActor*> Overlaps;
	GetOwner()->GetOverlappingActors(Overlaps, ASovCorruptionSourceVolume::StaticClass());
	TSet<FName> ActualContacts;
	for (AActor* Actor : Overlaps)
	{
		const auto* Source = Cast<ASovCorruptionSourceVolume>(Actor);
		float Falloff;
		if (IsValid(Source) && IsValid(Source->GetCorruptionProfile()) && Source->ValidateContact(GetOwner(), Falloff))
		{
			ActualContacts.Add(Source->GetCorruptionProfile()->SourceId);
		}
	}
	for (auto It = RestoredContactProfiles.CreateIterator(); It; ++It)
	{
		const FName SourceId = *It;
		if (!ActualContacts.Contains(SourceId) || !Records.ContainsByPredicate([SourceId](const auto& Record)
			{ return IsValid(Record.Profile) && Record.Profile->SourceId == SourceId; })) { It.RemoveCurrent(); }
	}
}
bool USovCorruptionComponent::FinishCampaignRestore(const USovCampaignDefinition* Destination, FString& OutError)
{
	const auto* StateComponent = Campaign();
	FString DefinitionError;
	if (bMutating || !ValidOwner() || !ValidBandTuning() || !IsValid(Destination)
		|| !StateComponent || !StateComponent->IsStateValid() || StateComponent->GetActiveMission() != Destination
		|| !Destination->ValidateDefinition(DefinitionError))
	{
		OutError = TEXT("Corruption restore requires a live owned ability system and the validated destination campaign state.");
		return false;
	}
	const auto* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
	FGameplayTagContainer OwnerTags;
	ASC->GetOwnedGameplayTags(OwnerTags);
	if (!OwnerTags.HasTagExact(Destination->Protagonist))
	{
		OutError = TEXT("Corruption restore protagonist does not match the destination mission."); return false;
	}
	if (!bRestoredDataValid)
	{
		OutError = TEXT("Corruption snapshot has an invalid schema, source contract or exposure record."); return false;
	}
	for (const auto& Record : Records)
	{
		if (Record.MissionId == Destination->MissionId
			&& (!IsValid(Record.Profile) || !Record.Profile->AllowedProtagonists.HasTagExact(Destination->Protagonist)))
		{
			OutError = TEXT("Corruption snapshot contains a source that does not permit this protagonist."); return false;
		}
	}
	TGuardValue<bool> MutationGuard(bMutating, true);
	// Exposure is mission-scoped; an independently saved protagonist cannot carry an old mission's field into the next.
	Records.RemoveAll([Destination](const auto& Record) { return Record.MissionId != Destination->MissionId; });
	for (auto It = RestoredContactProfiles.CreateIterator(); It; ++It)
	{
		const FName SourceId = *It;
		if (!Records.ContainsByPredicate([SourceId](const auto& Record)
			{ return IsValid(Record.Profile) && Record.Profile->SourceId == SourceId; })) { It.RemoveCurrent(); }
	}
	bWaitingForMissionRestore = false;
	PruneRestoredContacts(); // Suppression belongs to continuing physical contact, never a later re-entry.
	RefreshState(false); // Load restores mechanics; it never replays a canon consequence.
	if (!ValidOwner() || Campaign() != StateComponent || StateComponent->GetActiveMission() != Destination)
	{
		OutError = TEXT("Corruption ownership changed during restored band notification."); return false;
	}
	if (State.Band != ESovCorruptionBand::Clear && (!BandEffect.IsValid() || !EffectASC.IsValid()
		|| !EffectASC->GetActiveGameplayEffect(BandEffect)))
	{
		OutError = TEXT("Corruption could not restore its owned gameplay band effect."); return false;
	}
	OutError.Reset(); return true;
}
void USovCorruptionComponent::Load_Implementation()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bMutating || bEnding) { return; }
	{
		TGuardValue<bool> MutationGuard(bMutating, true);
		Sources.Empty(); Records.Empty(); RestoredContactProfiles.Empty();
		bool bValid = SavedSchemaVersion == 1 && SavedBand <= ESovCorruptionBand::OverwriteRisk && SavedRecords.Num() <= 64;
		float Total = 0.0f;
		TSet<FName> Seen;
		for (const auto& Record : SavedRecords)
		{
			ESovCorruptionBand Cap;
			if (!IsValid(Record.Profile) || !Record.Profile->bPersistExposureAtCheckpoint
				|| !Record.Profile->PermissionForMission(Record.MissionId, Cap) || Seen.Contains(Record.Profile->SourceId)
				|| !FMath::IsFinite(Record.Exposure) || Record.Exposure <= 0.0f || Record.Exposure > 100.0f)
			{
				bValid = false; break;
			}
			Seen.Add(Record.Profile->SourceId); Total += Record.Exposure;
		}
		bValid &= Total <= 100.0f;
		bRestoredDataValid = bValid;
		if (bValid)
		{
			Records = SavedRecords;
			for (const auto& Record : Records)
			{
				if (Record.bContactAtCheckpoint) { RestoredContactProfiles.Add(Record.Profile->SourceId); }
			}
		}
		else { SavedRecords.Empty(); SavedBand = ESovCorruptionBand::Clear; }
		RestoreBandSeed = SavedBand;
		bWaitingForMissionRestore = true;
		bForceBandEffectRefresh = true;
		// Pawn records load before the controller's destination mission record during a handoff.
		// Never filter records or replace the saved hysteresis seed using that temporary source/null mission.
		const auto Previous = State;
		State = FSovCorruptionReplicatedState();
		RemoveOwnedBandEffect();
		if (!bRestoredDataValid && !bEnding) { OnRep_State(Previous); }
	}
	// Encounter retry may load a component on an already-ready pawn; restore synchronously in that case.
	const auto* Player = Cast<ASovPlayerCharacterBase>(GetOwner());
	const auto* StateComponent = Campaign();
	if (Player && Player->IsCharacterReady() && StateComponent && !bEnding)
	{
		FString Error;
		FinishCampaignRestore(StateComponent->GetActiveMission(), Error);
	}
}
void USovCorruptionComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	bEnding = true; Sources.Empty(); RestoredContactProfiles.Empty(); RemoveOwnedBandEffect();
	Super::EndPlay(Reason);
}
