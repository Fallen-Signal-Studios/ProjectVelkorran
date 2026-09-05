// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Diagnostics/SovDiagnosticsSubsystem.h"
#include "Diagnostics/SovDiagnosticsPolicy.h"
#include "GameFramework/Pawn.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Components/SovEchoComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

bool USovDiagnosticsSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{ return WorldType == EWorldType::Game || WorldType == EWorldType::PIE; }
void USovDiagnosticsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	BoundSettings = USovGameUserSettings::Get();
	if (BoundSettings.IsValid()) { BoundSettings->OnUserSettingsChanged.AddDynamic(this, &ThisClass::HandleSettings); }
}
void USovDiagnosticsSubsystem::Deinitialize()
{
	UnbindPlayer();
	if (BoundSettings.IsValid()) { BoundSettings->OnUserSettingsChanged.RemoveDynamic(this, &ThisClass::HandleSettings); }
	ClearRecords(); Super::Deinitialize();
}
bool USovDiagnosticsSubsystem::IsRecordingEnabled() const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	const USovGameUserSettings* Settings = USovGameUserSettings::Get();
	return Settings && Settings->IsLocalDiagnosticsEnabled();
#endif
}
void USovDiagnosticsSubsystem::Tick(float DeltaTime)
{
	BindingElapsed += DeltaTime;
	if (BindingElapsed >= .25f) { BindingElapsed = 0.f; RefreshBindings(); }
	if (!IsRecordingEnabled() && !Records.IsEmpty()) { ClearRecords(); }
}
bool USovDiagnosticsSubsystem::IsSafeDebugId(FName Id)
{
	if (Id.IsNone()) { return true; }
	const FString Value = Id.ToString();
	return SovDiagnosticsPolicy::SafeId(*Value, static_cast<std::size_t>(Value.Len()));
}
void USovDiagnosticsSubsystem::Append(FSovDiagnosticRecord Value)
{
	if (!IsRecordingEnabled() || !IsSafeDebugId(Value.SourceId) || !IsSafeDebugId(Value.ContextId)
		|| !FMath::IsFinite(Value.Amount) || !FMath::IsFinite(Value.Remaining)) { return; }
	Value.Seconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (SovDiagnosticsPolicy::DropOldestBeforeAppend(static_cast<std::size_t>(Records.Num()))) { Records.RemoveAt(0, 1, EAllowShrinking::No); }
	Records.Add(Value);
}
void USovDiagnosticsSubsystem::Record(UWorld* World, ESovDiagnosticKind Kind, FName SourceId, FName ContextId,
	float Amount, float Remaining, bool bSucceeded)
{
	if (!World) { return; }
	if (USovDiagnosticsSubsystem* Subsystem = World->GetSubsystem<USovDiagnosticsSubsystem>())
	{
		FSovDiagnosticRecord Value; Value.Kind = Kind; Value.SourceId = SourceId; Value.ContextId = ContextId;
		Value.Amount = Amount; Value.Remaining = Remaining; Value.bSucceeded = bSucceeded; Subsystem->Append(Value);
	}
}
void USovDiagnosticsSubsystem::ClearRecords() { Records.Reset(); RecentDamageIds.Reset(); }
void USovDiagnosticsSubsystem::UnbindPlayer()
{
	if (BoundASC.IsValid())
	{
		BoundASC->OnDamageResolvedAsTarget.RemoveDynamic(this, &ThisClass::HandleDamage);
		BoundASC->OnDamageResolvedAsSource.RemoveDynamic(this, &ThisClass::HandleDamage);
		BoundASC->AbilityActivatedCallbacks.Remove(AbilityBeginHandle);
		BoundASC->AbilityCommittedCallbacks.Remove(AbilityCommitHandle);
		BoundASC->OnAbilityEnded.Remove(AbilityEndHandle);
		BoundASC->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetHealthAttribute()).Remove(HealthHandle);
	}
	if (BoundEcho.IsValid())
	{
		BoundEcho->OnEchoGranted.RemoveDynamic(this, &ThisClass::HandleEchoGain);
		BoundEcho->OnEchoSpent.RemoveDynamic(this, &ThisClass::HandleEchoSpend);
		BoundEcho->OnResonantStateChanged.RemoveDynamic(this, &ThisClass::HandleEchoThreshold);
	}
	if (BoundCampaign.IsValid())
	{
		BoundCampaign->OnMissionChanged.RemoveDynamic(this, &ThisClass::HandleMission);
		BoundCampaign->OnBeatCommitted.RemoveDynamic(this, &ThisClass::HandleBeat);
		BoundCampaign->OnEvidenceRecorded.RemoveDynamic(this, &ThisClass::HandleEvidence);
	}
	BoundASC.Reset(); BoundEcho.Reset(); BoundCampaign.Reset(); RecentDamageIds.Reset();
}
void USovDiagnosticsSubsystem::RefreshBindings()
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	UNarrativeAbilitySystemComponent* ASC = Pawn ? Cast<UNarrativeAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Pawn)) : nullptr;
	if (ASC && ASC->GetCharacterReadyEpoch() <= 0) { ASC = nullptr; }
	USovEchoComponent* Echo = ASC && Pawn ? Pawn->FindComponentByClass<USovEchoComponent>() : nullptr;
	USovCampaignStateComponent* Campaign = PC ? PC->FindComponentByClass<USovCampaignStateComponent>() : nullptr;
	if (BoundASC.Get() == ASC && BoundEcho.Get() == Echo && BoundCampaign.Get() == Campaign) { return; }
	UnbindPlayer(); BoundASC = ASC; BoundEcho = Echo; BoundCampaign = Campaign;
	if (ASC)
	{
		ASC->OnDamageResolvedAsTarget.AddDynamic(this, &ThisClass::HandleDamage);
		ASC->OnDamageResolvedAsSource.AddDynamic(this, &ThisClass::HandleDamage);
		AbilityBeginHandle = ASC->AbilityActivatedCallbacks.AddUObject(this, &ThisClass::HandleAbilityBegin);
		AbilityCommitHandle = ASC->AbilityCommittedCallbacks.AddUObject(this, &ThisClass::HandleAbilityCommit);
		AbilityEndHandle = ASC->OnAbilityEnded.AddUObject(this, &ThisClass::HandleAbilityEnd);
		HealthHandle = ASC->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetHealthAttribute()).AddUObject(this, &ThisClass::HandleHealth);
	}
	if (Echo)
	{
		Echo->OnEchoGranted.AddDynamic(this, &ThisClass::HandleEchoGain);
		Echo->OnEchoSpent.AddDynamic(this, &ThisClass::HandleEchoSpend);
		Echo->OnResonantStateChanged.AddDynamic(this, &ThisClass::HandleEchoThreshold);
	}
	if (Campaign)
	{
		Campaign->OnMissionChanged.AddDynamic(this, &ThisClass::HandleMission);
		Campaign->OnBeatCommitted.AddDynamic(this, &ThisClass::HandleBeat);
		Campaign->OnEvidenceRecorded.AddDynamic(this, &ThisClass::HandleEvidence);
	}
}
void USovDiagnosticsSubsystem::HandleDamage(const FSovDamageResult& Result)
{
	if (!IsRecordingEnabled() || !Result.TransactionId.IsValid() || RecentDamageIds.Contains(Result.TransactionId)) { return; }
	if (RecentDamageIds.Num() >= static_cast<int32>(SovDiagnosticsPolicy::MaximumReceiptIds)) { RecentDamageIds.RemoveAt(0, 1, EAllowShrinking::No); }
	RecentDamageIds.Add(Result.TransactionId);
	const FName Source = Result.SourceActor ? Result.SourceActor->GetClass()->GetFName() : FName(TEXT("Environment"));
	const FName Target = Result.TargetActor ? Result.TargetActor->GetClass()->GetFName() : NAME_None;
	Record(GetWorld(), ESovDiagnosticKind::Damage, Source, Target, Result.AppliedHealthDamage, Result.AppliedShieldDamage, Result.bPerfectDefense);
	if (Result.bFatal) { Record(GetWorld(), ESovDiagnosticKind::Fatal, Source, Target, Result.AppliedHealthDamage); }
	if (Result.bShieldBroken) { Record(GetWorld(), ESovDiagnosticKind::ShieldBreak, Source, Target); }
	if (Result.bPoiseBroken) { Record(GetWorld(), ESovDiagnosticKind::PoiseBreak, Source, Target); }
}
void USovDiagnosticsSubsystem::HandleEchoGain(FGameplayTag Source, float Amount, float Remaining)
{ Record(GetWorld(), ESovDiagnosticKind::EchoGain, Source.GetTagName(), NAME_None, Amount, Remaining, true); }
void USovDiagnosticsSubsystem::HandleEchoSpend(FGameplayTag Source, float Amount, float Remaining)
{ Record(GetWorld(), ESovDiagnosticKind::EchoSpend, Source.GetTagName(), NAME_None, Amount, Remaining, true); }
void USovDiagnosticsSubsystem::HandleEchoThreshold(bool bActive, float Current)
{ Record(GetWorld(), ESovDiagnosticKind::EchoThreshold, TEXT("Resonant"), NAME_None, Current, 0.f, bActive); }
void USovDiagnosticsSubsystem::HandleSettings(const FSovUserSettingsSnapshot& Value)
{
	if (!IsRecordingEnabled()) { ClearRecords(); return; }
	Record(GetWorld(), ESovDiagnosticKind::Settings, USovGameUserSettings::Get()->GetDifficultyId(), TEXT("Damage.Recovery"), Value.IncomingDamageScale, Value.EnemyRecoveryScale, true);
	Record(GetWorld(), ESovDiagnosticKind::Settings, TEXT("Assists"), TEXT("Defense.Exertion"), Value.DefenseWindowScale, Value.ExertionCostScale, true);
}
void USovDiagnosticsSubsystem::HandleMission(FName MissionId, bool bSucceeded)
{
	Record(GetWorld(), ESovDiagnosticKind::MissionState, MissionId, NAME_None, 0.f, 0.f, bSucceeded);
}
void USovDiagnosticsSubsystem::HandleBeat(const FSovCampaignJournalEntry& Entry)
{ Record(GetWorld(), ESovDiagnosticKind::BeatCommit, Entry.BeatId, Entry.MissionId, 0.f, 0.f, true); }
void USovDiagnosticsSubsystem::HandleEvidence(const FSovEvidenceAcquisition& Evidence)
{ Record(GetWorld(), ESovDiagnosticKind::Evidence, Evidence.EvidenceId, Evidence.MissionId, 0.f, 0.f, true); }
void USovDiagnosticsSubsystem::HandleAbilityBegin(UGameplayAbility* Ability)
{ if (IsValid(Ability)) { Record(GetWorld(), ESovDiagnosticKind::AbilityBegin, Ability->GetClass()->GetFName()); } }
void USovDiagnosticsSubsystem::HandleAbilityCommit(UGameplayAbility* Ability)
{ if (IsValid(Ability)) { Record(GetWorld(), ESovDiagnosticKind::AbilityCommit, Ability->GetClass()->GetFName(), NAME_None, 0.f, 0.f, true); } }
void USovDiagnosticsSubsystem::HandleAbilityEnd(const FAbilityEndedData& Data)
{ if (IsValid(Data.AbilityThatEnded)) { Record(GetWorld(), Data.bWasCancelled ? ESovDiagnosticKind::AbilityCancel : ESovDiagnosticKind::AbilityEnd, Data.AbilityThatEnded->GetClass()->GetFName()); } }
void USovDiagnosticsSubsystem::HandleHealth(const FOnAttributeChangeData& Change)
{ if (BoundASC.IsValid() && BoundASC->GetCharacterReadyEpoch() > 0 && Change.NewValue > Change.OldValue) { Record(GetWorld(), ESovDiagnosticKind::HealthRecovery, TEXT("Player"), NAME_None, Change.NewValue - Change.OldValue, Change.NewValue, true); } }
bool USovDiagnosticsSubsystem::ExportLocalReport(FString& OutRelativePath, FString& Error) const
{
	OutRelativePath.Reset();
	if (!IsRecordingEnabled()) { Error = TEXT("Local diagnostics are disabled."); return false; }
	FString Text = TEXT("Sovereign local diagnostics schema 1\nNo account IDs, free text, actor instance names or device identifiers.\nseconds\tkind\tsource\tcontext\tamount\tremaining\tsuccess\n");
	for (const FSovDiagnosticRecord& Value : Records)
	{
		Text += FString::Printf(TEXT("%.3f\t%d\t%s\t%s\t%.3f\t%.3f\t%d\n"), Value.Seconds, static_cast<int32>(Value.Kind),
			*Value.SourceId.ToString(), *Value.ContextId.ToString(), Value.Amount, Value.Remaining, Value.bSucceeded ? 1 : 0);
	}
	const FString Relative = FString::Printf(TEXT("Diagnostics/Sov-%s.tsv"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	const FString Path = FPaths::ProjectSavedDir() / Relative;
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	if (!FFileHelper::SaveStringToFile(Text, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{ Error = TEXT("Could not write the local diagnostics report."); return false; }
	OutRelativePath = Relative; Error.Reset(); return true;
}
