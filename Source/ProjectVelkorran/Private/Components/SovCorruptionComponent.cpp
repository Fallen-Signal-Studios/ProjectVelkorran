// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Components/SovCorruptionComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Corruption/SovCorruptionMath.h"
#include "Corruption/SovCorruptionSourceVolume.h"
#include "Corruption/SovCorruptionSourceComponent.h"
#include "Campaign/SovEncounterDirector.h"
#include "EngineUtils.h"
#include "UnrealFramework/NarrativeGameUserSettings.h"
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
bool USovCorruptionComponent::IsCombatPressureImmune() const
{
	const auto* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
	return ASC && (ASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Damage_Immune)
		|| ASC->HasMatchingGameplayTag(FSovGameplayTags::Get().Damage_Immunity_Corruption));
}
bool USovCorruptionComponent::ValidateProfilePermission(const USovCorruptionProfile* Profile,
	ESovCorruptionBand& OutCap, FName& OutMission) const
{
	OutCap = ESovCorruptionBand::Clear; OutMission = NAME_None;
	if (bWaitingForMissionRestore || !ValidOwner() || !ValidBandTuning()) { return false; }
	const auto* StateComponent = Campaign();
	const auto* Mission = StateComponent && StateComponent->IsStateValid() ? StateComponent->GetActiveMission() : nullptr;
	if (!IsValid(Profile) || !IsValid(Mission) || !Profile->PermissionForMission(Mission->MissionId, OutCap)) { return false; }
	if (!Profile->EscapeBeatId.IsNone() && (!Mission->FindBeat(Profile->EscapeBeatId)
		|| StateComponent->IsBeatComplete(Mission->MissionId, Profile->EscapeBeatId))) { return false; }
	const auto* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
	FGameplayTagContainer Owned; ASC->GetOwnedGameplayTags(Owned);
	const auto ActiveIdentity = StateComponent->GetActiveProtagonist();
	if (!Profile->AllowedProtagonists.HasTagExact(ActiveIdentity) || !Owned.HasTagExact(ActiveIdentity)) { return false; }
	if (Profile->bCanonPersistent)
	{
		const FSovCampaignBeatDefinition* Beat = Mission->FindBeat(Profile->ConsequenceBeatId);
		if (!Beat || Beat->bInteractiveChoice || !Beat->CinematicId.IsNone()) { return false; }
	}
	OutMission = Mission->MissionId;
	return true;
}
bool USovCorruptionComponent::ValidateSourcePermission(ASovCorruptionSourceVolume* Source,
	ESovCorruptionBand& OutCap, FName& OutMission) const
{
	OutCap = ESovCorruptionBand::Clear; OutMission = NAME_None;
	float Falloff;
	return IsValid(Source) && Source->GetWorld() == GetWorld() && !IsCombatPressureImmune()
		&& ValidateProfilePermission(Source->GetCorruptionProfile(), OutCap, OutMission)
		&& Source->ValidateContact(GetOwner(), Falloff);
}
bool USovCorruptionComponent::HasCompatibleProfile(const USovCorruptionProfile* Profile) const
{
	if (!IsValid(Profile)) { return false; }
	for (const auto& Pair : Sources)
	{
		const auto* Other = Pair.Value.Profile.Get();
		if (Other && Other != Profile && Other->SourceId == Profile->SourceId) { return false; }
	}
	return !Records.ContainsByPredicate([Profile](const auto& Record)
		{ return IsValid(Record.Profile) && Record.Profile != Profile && Record.Profile->SourceId == Profile->SourceId; });
}
FSovCorruptionSourceHandle USovCorruptionComponent::AcquireProducer(USovCorruptionSourceComponent* Producer)
{
	FSovCorruptionSourceHandle Handle;
	ESovCorruptionBand Cap; FName Mission; float Falloff;
	if (bMutating || !IsValid(Producer) || Producer->GetWorld() != GetWorld() || IsCombatPressureImmune()
		|| !ValidateProfilePermission(Producer->Profile, Cap, Mission) || !HasCompatibleProfile(Producer->Profile)
		|| !Producer->ValidateContact(GetOwner(), Falloff)) { return Handle; }
	bool bSameContact = false;
	for (const auto& Pair : Sources)
	{
		if (Pair.Value.Producer.Get() == Producer && Pair.Value.Profile.Get() == Producer->Profile.Get() && Pair.Value.MissionId == Mission)
		{
			Handle.Id = Pair.Key; return Handle;
		}
		bSameContact |= Pair.Value.Profile.Get() == Producer->Profile.Get();
	}
	TGuardValue<bool> Guard(bMutating, true);
	Handle.Id = FGuid::NewGuid();
	FSourceEntry Entry; Entry.Producer = Producer; Entry.Profile = Producer->Profile; Entry.MissionId = Mission;
	Sources.Add(Handle.Id, Entry);
	const bool bRestored = RestoredContactProfiles.Remove(Producer->Profile->SourceId) > 0;
	if (!bSameContact && !bRestored) { Accumulate(Producer->Profile, Mission, Producer->Profile->ContactExposure * Falloff, Cap); }
	RefreshState(true);
	if (bEnding || !IsValid(Producer)) { Sources.Remove(Handle.Id); Handle.Id.Invalidate(); }
	return Handle;
}
void USovCorruptionComponent::ReleaseProducer(FSovCorruptionSourceHandle Handle, USovCorruptionSourceComponent* Producer)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) { return; }
	const auto* Entry = Sources.Find(Handle.Id);
	if (Entry && Entry->Producer.Get() == Producer) { Sources.Remove(Handle.Id); }
}
bool USovCorruptionComponent::ApplyVerifiedPulse(USovCorruptionProfile* Profile, AActor* Source, float Falloff)
{
	ESovCorruptionBand Cap; FName Mission;
	if (bMutating || !IsValid(Source) || Source->GetWorld() != GetWorld() || IsCombatPressureImmune()
		|| !FMath::IsFinite(Falloff) || Falloff <= 0.0f || Falloff > 1.0f
		|| !ValidateProfilePermission(Profile, Cap, Mission) || !HasCompatibleProfile(Profile)) { return false; }
	TGuardValue<bool> Guard(bMutating, true);
	Records.RemoveAll([Mission](const auto& Record) { return Record.MissionId != Mission; });
	Accumulate(Profile, Mission, Profile->ContactExposure * Falloff, Cap);
	RefreshState(true); return !bEnding;
}
bool USovCorruptionComponent::ApplyVerifiedRemedy(USovCorruptionProfile* Profile, ESovCorruptionEscape Remedy)
{
	ESovCorruptionBand Cap; FName Mission;
	return IsValid(Profile) && Profile->Escape == Remedy && HasCompatibleProfile(Profile)
		&& ValidateProfilePermission(Profile, Cap, Mission) && CleanseExposure(100.0f, Profile->SourceId);
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
		ESovCorruptionBand Cap; FName SourceMission; float Falloff = 0.0f;
		auto* Profile = It.Value().Profile.Get();
		auto* Source = It.Value().Source.Get();
		auto* Producer = It.Value().Producer.Get();
		const bool bValidContact = Source
			? ValidateSourcePermission(Source, Cap, SourceMission) && Source->GetCorruptionProfile() == Profile && Source->ValidateContact(GetOwner(), Falloff)
			: IsValid(Producer) && Producer->Profile == Profile && !IsCombatPressureImmune()
				&& ValidateProfilePermission(Profile, Cap, SourceMission) && Producer->ValidateContact(GetOwner(), Falloff);
		if (!bValidContact || It.Value().MissionId != SourceMission) { It.RemoveCurrent(); continue; }
		ContactProfiles.Add(Profile);
		Accumulate(Profile, MissionId, Profile->ExposurePerSecond * Elapsed * Falloff, Cap);
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
	if (!bEnding) { AdvanceOverwrite(Elapsed); }
}
void USovCorruptionComponent::AdvanceOverwrite(float DeltaSeconds)
{
	if (!ValidOwner() || bWaitingForMissionRestore) { return; }
	const auto* CurrentCampaign = Campaign();
	const auto* Mission = CurrentCampaign ? CurrentCampaign->GetActiveMission() : nullptr;
	if (!Mission) { return; }
	const auto Previous = State;
	State.bOverwriteClockActive = false;
	State.OverwriteSecondsRemaining = 0.0f;
	for (auto& Record : Records)
	{
		const auto* Profile = Record.Profile.Get();
		if (!IsValid(Profile) || Record.MissionId != Mission->MissionId) { continue; }
		const auto* Permission = Profile->MissionPermissions.FindByPredicate([Mission](const auto& Item) { return Item.MissionId == Mission->MissionId; });
		if (!Permission || !Permission->bAllowOverwriteEncounterFailure || Record.bOverwriteTriggered) { continue; }
		const bool bEligible = Record.Exposure >= OverwriteThreshold && State.Band == ESovCorruptionBand::OverwriteRisk;
		if (!bEligible) { Record.OverwriteElapsed = 0.0f; continue; }
		if (IsCombatPressureImmune()) { continue; } // Immunity pauses a valid clock; it never silently cleanses a story fact.
		ASovEncounterDirector* Director = nullptr;
		for (TActorIterator<ASovEncounterDirector> It(GetWorld()); It; ++It)
		{
			if (It->EncounterId == Permission->OverwriteEncounterId && It->GetEncounterState() == ESovEncounterState::Active
				&& It->HasEncounterPlayer(GetOwner()))
			{
				if (Director) { Director = nullptr; break; } // Ambiguous authored identity fails closed.
				Director = *It;
			}
		}
		if (!Director) { continue; }
		Record.OverwriteElapsed = SovCorruptionMath::AdvanceOverwrite(Record.OverwriteElapsed, DeltaSeconds, Permission->OverwriteSeconds, true);
		const float Remaining = FMath::Max(0.0f, Permission->OverwriteSeconds - Record.OverwriteElapsed);
		State.OverwriteSecondsRemaining = State.bOverwriteClockActive ? FMath::Min(State.OverwriteSecondsRemaining, Remaining) : Remaining;
		State.bOverwriteClockActive = true;
		if (Remaining <= 0.0f)
		{
			Record.bOverwriteTriggered = true;
			RefreshSavedSnapshot(); // Save observers see the committed clock and cannot resurrect an expired timer.
			OnRep_State(Previous); // Explicit zero-time mechanical feedback precedes encounter failure.
			if (ValidOwner() && Campaign() == CurrentCampaign && CurrentCampaign->GetActiveMission() == Mission
				&& IsValid(Director) && Director->HasEncounterPlayer(GetOwner()) && !IsCombatPressureImmune())
			{
				Director->FailEncounter();
			}
			return;
		}
	}
	RefreshSavedSnapshot();
	if (Previous.bOverwriteClockActive != State.bOverwriteClockActive || Previous.OverwriteSecondsRemaining != State.OverwriteSecondsRemaining)
	{
		OnRep_State(Previous);
	}
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
	Next.bOverwriteClockActive = State.bOverwriteClockActive;
	Next.OverwriteSecondsRemaining = State.OverwriteSecondsRemaining;
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
	if (Next.Band != ESovCorruptionBand::OverwriteRisk)
	{
		Next.bOverwriteClockActive = false; Next.OverwriteSecondsRemaining = 0.0f;
		for (auto& Record : Records) { Record.OverwriteElapsed = 0.0f; Record.bOverwriteTriggered = false; }
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
	float Resistance = 0.0f, RegenScale = 1.0f;
	for (const auto& Profile : State.Profiles)
	{
		ESovCorruptionBand ProfileCap;
		if (!Mission || !Profile->PermissionForMission(Mission->MissionId, ProfileCap)) { continue; }
		if (State.Band >= ESovCorruptionBand::Intrusion && ProfileCap >= ESovCorruptionBand::Intrusion) { Resistance = FMath::Min(Resistance, -Profile->IntrusionVulnerability); }
		if (State.Band >= ESovCorruptionBand::Contest && ProfileCap >= ESovCorruptionBand::Contest) { RegenScale = FMath::Min(RegenScale, Profile->ContestStaminaRegenScale); }
	}
	const bool bImmune = IsCombatPressureImmune();
	if (bForceBandEffectRefresh || AppliedEffectBand != State.Band || EffectASC.Get() != ASC
		|| AppliedResistance != Resistance || AppliedRegenScale != RegenScale || (bImmune && BandEffect.IsValid())
		|| (bBandEffectMissing && State.Band != ESovCorruptionBand::Clear && !bImmune))
	{
		bForceBandEffectRefresh = false;
		RemoveOwnedBandEffect();
		if (bEnding) { return; }
		if (IsValid(ASC) && ValidOwner() && ASC->GetAvatarActor() == GetOwner()
			&& UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()) == ASC
			&& State.Band != ESovCorruptionBand::Clear && !bImmune)
		{
			const auto& Tags = FSovGameplayTags::Get();
			const FGameplayTag BandTags[] = {FGameplayTag(), Tags.State_Corruption_Trace, Tags.State_Corruption_Intrusion,
				Tags.State_Corruption_Contest, Tags.State_Corruption_OverwriteRisk};
			auto Spec = ASC->MakeOutgoingSpec(USovGameplayEffect_CorruptionBand::StaticClass(), 1.0f, ASC->MakeEffectContext());
			if (Spec.IsValid())
			{
				Spec.Data->DynamicGrantedTags.AddTag(BandTags[static_cast<uint8>(State.Band)]);
				Spec.Data->SetSetByCallerMagnitude(FName(TEXT("Corruption.Resistance")), Resistance);
				Spec.Data->SetSetByCallerMagnitude(FName(TEXT("Corruption.StaminaRegenScale")), RegenScale);
				const auto Applied = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
				if (bEnding || !ValidOwner() || UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()) != ASC)
				{
					if (IsValid(ASC) && Applied.IsValid()) { ASC->RemoveActiveGameplayEffect(Applied); }
					return;
				}
				BandEffect = Applied; EffectASC = ASC; AppliedEffectBand = State.Band;
				AppliedResistance = Resistance; AppliedRegenScale = RegenScale;
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
float USovCorruptionComponent::ResolveIncomingStatusDuration(AActor* Target, float AuthoredDuration)
{
	if (!FMath::IsFinite(AuthoredDuration) || AuthoredDuration <= 0.0f) { return 0.0f; }
	const auto* Component = IsValid(Target) ? Target->FindComponentByClass<USovCorruptionComponent>() : nullptr;
	if (!Component || Component->bEnding || Component->bWaitingForMissionRestore || Component->IsCombatPressureImmune()
		|| !Component->EffectASC.IsValid() || Component->EffectASC->GetAvatarActor() != Target
		|| Component->EffectASC.Get() != UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target)
		|| !Component->BandEffect.IsValid() || !Component->EffectASC->GetActiveGameplayEffect(Component->BandEffect)) { return AuthoredDuration; }
	return static_cast<float>(FMath::Min(static_cast<double>(MAX_flt), static_cast<double>(AuthoredDuration)
		* SovCorruptionMath::StatusDurationScale(static_cast<int>(Component->AppliedEffectBand))));
}
FSovCorruptionPresentationRequest USovCorruptionComponent::GetPresentationRequest() const
{
	FSovCorruptionPresentationRequest Request;
	Request.Band = State.Band; Request.Exposure = State.Exposure;
	const auto* Settings = UNarrativeGameUserSettings::GetSovSettings();
	Request.bReducedEffects = bReducedEffects || (Settings && Settings->IsReducedCorruptionEffectsEnabled());
	Request.bOverwriteClockActive = State.bOverwriteClockActive;
	Request.OverwriteSecondsRemaining = State.OverwriteSecondsRemaining;
	for (const auto& Profile : State.Profiles)
	{
		if (!IsValid(Profile)) { continue; }
		Request.ProfileIds.Add(Profile->PresentationProfileId);
		Request.RemedyTexts.Add(Profile->RemedyText);
		Request.InformationTexts.Add(Request.bReducedEffects ? Profile->ReducedEffectsSubstitute : Profile->InformationText);
		if (!Request.bReducedEffects) { Request.SuggestedIntensity = FMath::Max(Request.SuggestedIntensity, Profile->PresentationIntensity); }
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
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		TInlineComponentArray<USovCorruptionSourceComponent*> Producers;
		It->GetComponents(Producers);
		for (const auto* Producer : Producers)
		{
			float Falloff;
			if (IsValid(Producer->Profile) && Producer->ValidateContact(GetOwner(), Falloff)) { ActualContacts.Add(Producer->Profile->SourceId); }
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
	if (!OwnerTags.HasTagExact(StateComponent->GetActiveProtagonist()))
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
			&& (!IsValid(Record.Profile) || !Record.Profile->AllowedProtagonists.HasTagExact(StateComponent->GetActiveProtagonist())))
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
	if (!bEnding) { AdvanceOverwrite(0.0f); }
	if (!ValidOwner() || Campaign() != StateComponent || StateComponent->GetActiveMission() != Destination)
	{
		OutError = TEXT("Corruption ownership changed during restored band notification."); return false;
	}
	if (State.Band != ESovCorruptionBand::Clear && !IsCombatPressureImmune() && (!BandEffect.IsValid() || !EffectASC.IsValid()
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
				|| !FMath::IsFinite(Record.Exposure) || Record.Exposure <= 0.0f || Record.Exposure > 100.0f
				|| !FMath::IsFinite(Record.OverwriteElapsed) || Record.OverwriteElapsed < 0.0f || Record.OverwriteElapsed > 600.0f)
			{
				bValid = false; break;
			}
			const auto* Permission = Record.Profile->MissionPermissions.FindByPredicate([&Record](const auto& Item) { return Item.MissionId == Record.MissionId; });
			if (!Permission || (Permission->bAllowOverwriteEncounterFailure
				? Record.OverwriteElapsed > Permission->OverwriteSeconds || (Record.bOverwriteTriggered && Record.OverwriteElapsed < Permission->OverwriteSeconds)
				: Record.OverwriteElapsed != 0.0f || Record.bOverwriteTriggered))
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
