// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Validation/SovValidateCampaignCommandlet.h"
#include "Campaign/SovCampaignDefinition.h"
#include "Framework/SovPlayerController.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"

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
		UE_LOG(LogTemp, Error, TEXT("Supply -Missions=/Game/Missions/DA_M01.DA_M01,/Game/Missions/DA_M02.DA_M02 (the complete shipping mission set)."));
		return 2;
	}
	TArray<FString> Paths;
	MissionArgument.ParseIntoArray(Paths, TEXT(","), true);
	TMap<FName, USovCampaignDefinition*> Missions;
	TSet<FPrimaryAssetId> PrimaryIds;
	int32 Errors = 0;
	for (FString& Path : Paths)
	{
		Path.TrimStartAndEndInline();
		USovCampaignDefinition* Mission = LoadObject<USovCampaignDefinition>(nullptr, *Path);
		FString Error;
		if (!Mission || !ASovPlayerController::ValidateMissionPawn(Mission, Error))
		{
			UE_LOG(LogTemp, Error, TEXT("%s: %s"), *Path, Mission ? *Error : TEXT("Missing mission asset or wrong asset type."));
			++Errors;
			continue;
		}
		const FPrimaryAssetId PrimaryId = Mission->GetPrimaryAssetId();
		if (!PrimaryId.IsValid() || PrimaryIds.Contains(PrimaryId))
		{ UE_LOG(LogTemp, Error, TEXT("%s: missing or duplicate primary asset ID."), *Path); ++Errors; }
		PrimaryIds.Add(PrimaryId);
		if (Missions.Contains(Mission->MissionId))
		{ UE_LOG(LogTemp, Error, TEXT("%s: duplicate mission ID %s."), *Path, *Mission->MissionId.ToString()); ++Errors; }
		else { Missions.Add(Mission->MissionId, Mission); }
		const FString MapPackage = Mission->Map.ToSoftObjectPath().GetLongPackageName();
		if (MapPackage.IsEmpty() || !FPackageName::DoesPackageExist(MapPackage))
		{ UE_LOG(LogTemp, Error, TEXT("%s: map package is missing."), *Path); ++Errors; }
		if (Mission->EntryPlayerStartTag.IsNone())
		{ UE_LOG(LogTemp, Error, TEXT("%s: campaign entry must identify an authored PlayerStart tag."), *Path); ++Errors; }
	}
	for (const TPair<FName, USovCampaignDefinition*>& Item : Missions)
	{
		for (FName Successor : Item.Value->AllowedSuccessorMissions)
		{
			if (!Missions.Contains(Successor))
			{ UE_LOG(LogTemp, Error, TEXT("%s: successor %s is absent from the manifest."), *Item.Key.ToString(), *Successor.ToString()); ++Errors; }
		}
	}
	if (!Missions.Contains(TEXT("M01_Mantle")) || !Missions.Contains(TEXT("M02_OneDegree")))
	{ UE_LOG(LogTemp, Error, TEXT("Opening campaign manifest must contain M01_Mantle and M02_OneDegree.")); ++Errors; }
	UE_LOG(LogTemp, Display, TEXT("Native mission preflight: %d assets, %d errors. Map actors, Blueprint compilation, dialogue, localization, cook dependencies and end-to-end playthroughs require their separate gates."), Missions.Num(), Errors);
	return Errors == 0 ? 0 : 1;
}
