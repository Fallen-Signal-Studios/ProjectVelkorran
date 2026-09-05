// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Validation/SovValidateCampaignCommandlet.h"
#include "Validation/SovCampaignDependencyPolicy.h"
#include "Campaign/SovCampaignDefinition.h"
#include "Framework/SovPlayerController.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Diagnostics/SovDiagnosticsSubsystem.h"
#include "Diagnostics/SovLogChannels.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Modules/ModuleManager.h"
#include "Internationalization/Text.h"
#include "Campaign/SovEvidenceDefinition.h"
#include "Corruption/SovCorruptionProfile.h"
#include "Melee/SovMeleeAttackDefinition.h"
#include "Narrative/SovNarrativeCue.h"
#include "Narrative/SovNarrativeValidationLibrary.h"
#include "Tales/Dialogue.h"
#include "Engine/Blueprint.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	bool HasStableText(const FText& Text)
	{
		FName TableId; FString Key;
		return !Text.IsEmpty() && FTextInspector::GetTableIdAndKey(Text, TableId, Key) && !TableId.IsNone() && !Key.IsEmpty();
	}
	int32 ValidateNativeAsset(UObject* Asset, const TMap<FName, USovCampaignDefinition*>& Missions, bool bShippingValidation,
		TSet<UObject*>& VisitedAssets, TMap<FName, UObject*>& EvidenceIds, TMap<FName, UObject*>& CueIds)
	{
		if (!IsValid(Asset) || VisitedAssets.Contains(Asset)) { return 0; }
		VisitedAssets.Add(Asset);
		int32 Errors = 0; FString Error;
		const auto Fail = [&](const FString& Message)
		{ UE_LOG(LogSovMission, Error, TEXT("%s: %s"), *Asset->GetPathName(), *Message); ++Errors; };
		const auto CheckText = [&](const FText& Text, const TCHAR* Field)
		{ if (bShippingValidation && !HasStableText(Text)) { Fail(FString(Field) + TEXT(" requires a non-empty string-table entry.")); } };
		const auto CheckDialogue = [&](UDialogue* Dialogue, USovCampaignDefinition* Mission)
		{
			TArray<FString> Issues, Warnings;
			USovNarrativeValidationLibrary::ValidateDialogue(Dialogue, Mission, bShippingValidation, Issues, Warnings);
			for (const FString& Issue : Issues) { Fail(Issue); }
			for (const FString& Warning : Warnings) { UE_LOG(LogSovMission, Warning, TEXT("%s: %s"), *Asset->GetPathName(), *Warning); }
		};
		if (auto* Cue = Cast<USovNarrativeCue>(Asset))
		{
			if (!Cue->Validate(Error)) { Fail(Error); }
			if (CueIds.Contains(Cue->CueId) && CueIds.FindChecked(Cue->CueId) != Cue) { Fail(TEXT("Duplicate narrative cue ID in the dependency closure.")); }
			else { CueIds.Add(Cue->CueId, Cue); }
			if (!Cue->RequiredMission.IsNone() && !Missions.Contains(Cue->RequiredMission)) { Fail(TEXT("Cue required mission is absent from the manifest.")); }
			for (const FSovBarkVariant& Variant : Cue->BarkVariants) { CheckText(Variant.Caption, TEXT("Bark caption")); }
			if (Cue->bRecordUnheardSummary) { CheckText(Cue->RecordSummary, TEXT("Unheard cue record summary")); }
			if (Cue->Dialogue) { CheckDialogue(Cue->Dialogue->GetDefaultObject<UDialogue>(), Missions.FindRef(Cue->RequiredMission)); }
		}
		else if (auto* Evidence = Cast<USovEvidenceDefinition>(Asset))
		{
			if (!Evidence->ValidateDefinition(Error)) { Fail(Error); }
			if (EvidenceIds.Contains(Evidence->EvidenceId) && EvidenceIds.FindChecked(Evidence->EvidenceId) != Evidence) { Fail(TEXT("Duplicate evidence ID in the dependency closure.")); }
			else { EvidenceIds.Add(Evidence->EvidenceId, Evidence); }
			CheckText(Evidence->Summary, TEXT("Evidence summary")); CheckText(Evidence->FullText, TEXT("Evidence full text"));
			if (!Evidence->ObservedFacts.IsEmpty()) { CheckText(Evidence->ObservedFacts,TEXT("Evidence observed facts")); }
			if (!Evidence->Interpretation.IsEmpty()) { CheckText(Evidence->Interpretation,TEXT("Evidence interpretation")); }
			for (FName Mission : Evidence->RelevantMissions) { if (!Missions.Contains(Mission)) { Fail(TEXT("Evidence relevant mission is absent from the manifest.")); } }
		}
		else if (auto* Melee = Cast<USovMeleeAttackDefinition>(Asset)) { if (!Melee->Validate(Error)) { Fail(Error); } }
		else if (auto* Corruption = Cast<USovCorruptionProfile>(Asset)) { if (!Corruption->ValidateProfile(Error)) { Fail(Error); } }
		else if (auto* Dialogue = Cast<UDialogue>(Asset)) { CheckDialogue(Dialogue, nullptr); }
		else if (auto* Blueprint = Cast<UBlueprint>(Asset))
		{
			if (Blueprint->GeneratedClass && Blueprint->GeneratedClass->IsChildOf(UDialogue::StaticClass()))
			{ CheckDialogue(Blueprint->GeneratedClass->GetDefaultObject<UDialogue>(), nullptr); }
		}
		return Errors;
	}
	int32 ValidateDependencyClosure(const TArray<FName>& RootPackages, const TMap<FName, USovCampaignDefinition*>& Missions, bool bShippingValidation)
	{
		FAssetRegistryModule& Module = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		IAssetRegistry& Registry = Module.Get(); Registry.SearchAllAssets(true);
		TArray<FName> Pending = RootPackages;
		TSet<FName> Visited; TSet<UObject*> VisitedAssets; TMap<FName, UObject*> EvidenceIds, CueIds;
		TArray<TStrongObjectPtr<UObject>> RetainedAssets;
		int32 Errors = 0;
		const TArray<FString> ExcludedRoots = { TEXT("/Operations/"), TEXT("/NarrativeOperations/"), TEXT("/Prototype/"), TEXT("/Prototypes/") };
		const TArray<FString> LegacySegments = { TEXT("/Crafting/"), TEXT("/Vendors/"), TEXT("/Morality/"), TEXT("/Rarity/") };
		while (!Pending.IsEmpty())
		{
			const FName Package = Pending.Pop(EAllowShrinking::No);
			if (Visited.Contains(Package)) { continue; }
			Visited.Add(Package);
			const FString Name = Package.ToString();
			bool bForbidden = false;
			for (const FString& Root : ExcludedRoots) { bForbidden |= Name.StartsWith(Root, ESearchCase::IgnoreCase); }
			for (const FString& Segment : LegacySegments) { bForbidden |= Name.Contains(Segment, ESearchCase::IgnoreCase); }
			if (bShippingValidation && bForbidden) { UE_LOG(LogSovMission, Error, TEXT("Campaign dependency enters excluded/legacy content: %s"), *Name); ++Errors; }
			if (!Name.StartsWith(TEXT("/Script/")) && !FPackageName::DoesPackageExist(Name))
			{ UE_LOG(LogSovMission, Error, TEXT("Campaign dependency package missing: %s"), *Name); ++Errors; continue; }
			// Script packages contain native defaults, not authored shipping asset instances.
			if (!Name.StartsWith(TEXT("/Script/")))
			{
				TArray<FAssetData> Assets; Registry.GetAssetsByPackageName(Package, Assets, true);
				for (const FAssetData& Data : Assets)
				{
					// Avoid loading bulk textures, meshes and world payloads solely to discover that they have no native validator.
					const UClass* KnownClass = FindObject<UClass>(nullptr, *Data.AssetClassPath.ToString());
					if (KnownClass && !KnownClass->IsChildOf(USovNarrativeCue::StaticClass())
						&& !KnownClass->IsChildOf(USovEvidenceDefinition::StaticClass()) && !KnownClass->IsChildOf(USovMeleeAttackDefinition::StaticClass())
						&& !KnownClass->IsChildOf(USovCorruptionProfile::StaticClass()) && !KnownClass->IsChildOf(UDialogue::StaticClass())
						&& !KnownClass->IsChildOf(UBlueprint::StaticClass())) { continue; }
					UObject* Asset = Data.GetAsset();
					if (!Asset) { UE_LOG(LogSovMission, Error, TEXT("Could not load dependency asset %s."), *Data.GetObjectPathString()); ++Errors; continue; }
					RetainedAssets.Emplace(Asset);
					Errors += ValidateNativeAsset(Asset, Missions, bShippingValidation, VisitedAssets, EvidenceIds, CueIds);
				}
			}
			TArray<FName> Dependencies;
			Registry.GetDependencies(Package, Dependencies, UE::AssetRegistry::EDependencyCategory::Package);
			Pending.Append(Dependencies);
		}
		for (const auto& Item : EvidenceIds)
		{
			const auto* Evidence = CastChecked<USovEvidenceDefinition>(Item.Value);
			for (FName Supporting : Evidence->SupportingEvidenceIds)
			{
				if (!EvidenceIds.Contains(Supporting))
				{ UE_LOG(LogSovMission, Error, TEXT("%s: supporting evidence %s is absent from the dependency closure. Include dynamic assets with -AdditionalAssets."), *Evidence->GetPathName(), *Supporting.ToString()); ++Errors; }
			}
		}
		UE_LOG(LogSovMission, Display, TEXT("Examined %d on-disk dependency packages. Dynamic string loads and semantic Blueprint behavior need separate validation."), Visited.Num());
		return Errors;
	}
}


USovValidateCampaignCommandlet::USovValidateCampaignCommandlet()
{
	IsClient = false;
	IsServer = true;
	IsEditor = true;
	LogToConsole = true;
}

int32 USovValidateCampaignCommandlet::Main(const FString& Params)
{
	FString MissionArgument;
	if (!FParse::Value(*Params, TEXT("Missions="), MissionArgument) || MissionArgument.IsEmpty())
	{
		UE_LOG(LogSovMission, Error, TEXT("Supply -Missions=/Game/Missions/DA_M01.DA_M01,/Game/Missions/DA_M02.DA_M02 (the complete shipping mission set)."));
		return 2;
	}
	TArray<FString> Paths;
	MissionArgument.ParseIntoArray(Paths, TEXT(","), true);
	TMap<FName, USovCampaignDefinition*> Missions;
	TArray<TStrongObjectPtr<UObject>> RetainedRoots;
	TSet<FPrimaryAssetId> PrimaryIds;
	TSet<FName> ConsequenceIds, MemoryIds;
	int32 Errors = 0;
	const bool bShippingValidation = FParse::Param(*Params, TEXT("ShippingValidation"));
	TArray<FName> RootPackages;
	for (FString& Path : Paths)
	{
		Path.TrimStartAndEndInline();
		USovCampaignDefinition* Mission = LoadObject<USovCampaignDefinition>(nullptr, *Path);
		if (Mission) { RetainedRoots.Emplace(Mission); }
		FString Error;
		if (!Mission || !ASovPlayerController::ValidateMissionPawn(Mission, Error))
		{
			UE_LOG(LogSovMission, Error, TEXT("%s: %s"), *Path, Mission ? *Error : TEXT("Missing mission asset or wrong asset type."));
			++Errors;
			continue;
		}
		RootPackages.Add(Mission->GetOutermost()->GetFName());
		if (!USovDiagnosticsSubsystem::IsSafeDebugId(Mission->MissionId))
		{ UE_LOG(LogSovMission, Error, TEXT("%s: mission debug ID must use at most 96 ASCII letters/digits/dot/dash/underscore."), *Path); ++Errors; }
		if (bShippingValidation && !HasStableText(Mission->DisplayName))
		{ UE_LOG(LogSovMission, Error, TEXT("%s: shipping mission name requires a non-empty string-table entry."), *Path); ++Errors; }
		for (const FSovCampaignBeatDefinition& Beat : Mission->Beats)
		{
			for (const auto& Consequence : Beat.Consequences)
			{
				if (ConsequenceIds.Contains(Consequence.ConsequenceId))
				{ UE_LOG(LogSovMission, Error, TEXT("%s/%s: consequence ID %s is reused in the campaign manifest."), *Path, *Beat.BeatId.ToString(), *Consequence.ConsequenceId.ToString()); ++Errors; }
				ConsequenceIds.Add(Consequence.ConsequenceId);
			}
			for (const auto& Memory : Beat.RelationshipMemories)
			{
				if (MemoryIds.Contains(Memory.MemoryId))
				{ UE_LOG(LogSovMission, Error, TEXT("%s/%s: relationship memory ID %s is reused in the campaign manifest."), *Path, *Beat.BeatId.ToString(), *Memory.MemoryId.ToString()); ++Errors; }
				MemoryIds.Add(Memory.MemoryId);
			}
			if (!USovDiagnosticsSubsystem::IsSafeDebugId(Beat.BeatId))
			{ UE_LOG(LogSovMission, Error, TEXT("%s: invalid beat debug ID %s."), *Path, *Beat.BeatId.ToString()); ++Errors; }
			if (bShippingValidation && !HasStableText(Beat.ObjectiveText))
			{ UE_LOG(LogSovMission, Error, TEXT("%s/%s: shipping objective requires a non-empty string-table entry."), *Path, *Beat.BeatId.ToString()); ++Errors; }
		}
		const FPrimaryAssetId PrimaryId = Mission->GetPrimaryAssetId();
		if (!PrimaryId.IsValid() || PrimaryIds.Contains(PrimaryId))
		{ UE_LOG(LogSovMission, Error, TEXT("%s: missing or duplicate primary asset ID."), *Path); ++Errors; }
		PrimaryIds.Add(PrimaryId);
		if (Missions.Contains(Mission->MissionId))
		{ UE_LOG(LogSovMission, Error, TEXT("%s: duplicate mission ID %s."), *Path, *Mission->MissionId.ToString()); ++Errors; }
		else { Missions.Add(Mission->MissionId, Mission); }
		const FString MapPackage = Mission->Map.ToSoftObjectPath().GetLongPackageName();
		if (MapPackage.IsEmpty() || !FPackageName::DoesPackageExist(MapPackage))
		{ UE_LOG(LogSovMission, Error, TEXT("%s: map package is missing."), *Path); ++Errors; }
		if (Mission->EntryPlayerStartTag.IsNone())
		{ UE_LOG(LogSovMission, Error, TEXT("%s: campaign entry must identify an authored PlayerStart tag."), *Path); ++Errors; }
	}
	for (const TPair<FName, USovCampaignDefinition*>& Item : Missions)
	{
		for (FName Successor : Item.Value->AllowedSuccessorMissions)
		{
			if (!Missions.Contains(Successor))
			{ UE_LOG(LogSovMission, Error, TEXT("%s: successor %s is absent from the manifest."), *Item.Key.ToString(), *Successor.ToString()); ++Errors; }
		}
	}
	// A required inherited consequence must be unavoidable before this mission can enter.
	TMap<FName, TPair<FName, bool>> ConsequenceWriters;
	TMap<FName, std::size_t> MissionIndices;
	for (const auto& Item : Missions) { MissionIndices.Add(Item.Key, static_cast<std::size_t>(MissionIndices.Num())); }
	std::vector<std::vector<std::size_t>> MissionEdges(static_cast<std::size_t>(MissionIndices.Num()));
	for (const auto& Item : Missions)
	{
		for (FName Successor : Item.Value->AllowedSuccessorMissions)
		{
			const auto* Index = MissionIndices.Find(Successor);
			// Out-of-manifest edges were reported above and make prerequisite proof fail closed.
			MissionEdges[MissionIndices.FindChecked(Item.Key)].push_back(Index ? *Index : MissionEdges.size());
		}
		for (const auto& Beat : Item.Value->Beats)
		{
			for (const auto& Consequence : Beat.Consequences)
			{ ConsequenceWriters.Add(Consequence.ConsequenceId, TPair<FName, bool>(Item.Key, !Beat.bOptional)); }
		}
	}
	for (const auto& Item : Missions)
	{
		for (FName Required : Item.Value->RequiredPriorConsequenceIds)
		{
			const auto* Writer = ConsequenceWriters.Find(Required);
			const bool bGuaranteed = Writer && SovCampaignDependencyPolicy::IsMandatoryPredecessor(MissionEdges,
				MissionIndices.FindChecked(Writer->Key), MissionIndices.FindChecked(Item.Key), Writer->Value);
			if (!bGuaranteed)
			{
				UE_LOG(LogSovMission, Error, TEXT("%s: inherited consequence %s needs a mandatory strict-predecessor writer on every manifest entry path; absent, optional, bypassable or rootless paths cannot gate canon."), *Item.Key.ToString(), *Required.ToString());
				++Errors;
			}
		}
	}
	if (!Missions.Contains(TEXT("M01_Mantle")) || !Missions.Contains(TEXT("M02_OneDegree")))
	{ UE_LOG(LogSovMission, Error, TEXT("Opening campaign manifest must contain M01_Mantle and M02_OneDegree.")); ++Errors; }
	FString AdditionalArgument;
	if (FParse::Value(*Params, TEXT("AdditionalAssets="), AdditionalArgument))
	{
		TArray<FString> AdditionalPaths; AdditionalArgument.ParseIntoArray(AdditionalPaths, TEXT(","), true);
		for (FString& Path : AdditionalPaths)
		{
			Path.TrimStartAndEndInline(); UObject* Asset = LoadObject<UObject>(nullptr, *Path);
			if (!Asset) { UE_LOG(LogSovMission, Error, TEXT("Additional asset could not be loaded: %s"), *Path); ++Errors; }
			else { RetainedRoots.Emplace(Asset); RootPackages.AddUnique(Asset->GetOutermost()->GetFName()); }
		}
	}
	Errors += ValidateDependencyClosure(RootPackages, Missions, bShippingValidation);
	UE_LOG(LogSovMission, Display, TEXT("Native mission preflight: %d assets, %d errors. Melee, corruption, evidence, cue and Narrative dialogue graph validators ran over dependency assets. Use -ShippingValidation for string-table IDs and excluded dependency roots; -AdditionalAssets includes dynamic-only content. Map actors, Blueprint compilation, translation coverage and playthroughs require separate gates."), Missions.Num(), Errors);
	return Errors == 0 ? 0 : 1;
}
