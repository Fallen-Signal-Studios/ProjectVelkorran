// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Character/PlayerDefinition.h"
#include "CharacterCreator/NarrativeSaveWithCreatorData.h"
#include "Framework/SovPlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "Tests/SovHandoffRuntimeTestFixtures.h"
#include "UnrealFramework/NarrativeGameMode.h"

#if WITH_AUTOMATION_TESTS && WITH_SERVER_CODE
struct FNarrativeTravelTestAccess
{
	static void Process(ANarrativeGameMode& Mode, const FString& URL)
	{
		Mode.ProcessServerTravel(URL, true);
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovNarrativeTravelSaveVetoTest,
	"ProjectVelkorran.Campaign.Travel.FailedSaveClearsPendingTravel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovNarrativeTravelSaveVetoTest::RunTest(const FString& Parameters)
{
	if (!GEngine) { AddError(TEXT("Engine unavailable")); return false; }
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("World"), World)) { return false; }
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeNewWorld(UWorld::InitializationValues().AllowAudioPlayback(false)
		.RequiresHitProxies(false).CreatePhysicsScene(false).CreateNavigation(false)
		.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false));
	ANarrativeGameMode* Mode = World->SpawnActor<ANarrativeGameMode>();
	APlayerController* Player = World->SpawnActor<APlayerController>();
	UNarrativeSaveSubsystem* Save = World->GetSubsystem<UNarrativeSaveSubsystem>();
	const bool bReady = TestNotNull(TEXT("Narrative mode"), Mode)
		&& TestNotNull(TEXT("Player"), Player) && TestNotNull(TEXT("Save subsystem"), Save)
		&& TestTrue(TEXT("Player is registered with this world"), World->GetFirstPlayerController() == Player);
	if (bReady)
	{
		// A blocked save fails before any disk IO, regardless of whether the test
		// controller has initialized its PlayerState. Simulate UWorld's pending URL.
		Save->SetSavingDisabled(true);
		World->NextURL = TEXT("/Game/UnavailableTravelTestMap?LevelTransition=1");
		World->NextSwitchCountdown = 5.f;
		AddExpectedError(TEXT("Travel aborted: player save failed"), EAutomationExpectedErrorFlags::Contains, 1);
		FNarrativeTravelTestAccess::Process(*Mode, World->NextURL);
		TestTrue(TEXT("Failed save removes scheduled world travel"), World->NextURL.IsEmpty());
		TestEqual(TEXT("Failed save clears the obsolete countdown"), World->NextSwitchCountdown, 0.f);
		TestTrue(TEXT("Player remains in original world"), IsValid(Player) && Player->GetWorld() == World);
	}
	// Never leave a test URL queued even if the regression assertion failed.
	World->NextURL.Reset();
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return bReady;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovNarrativePlayerSlotPreservesSubclassTest,
	"ProjectVelkorran.Campaign.Travel.PlayerSlotPreservesSaveSubclassData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovNarrativePlayerSlotPreservesSubclassTest::RunTest(const FString& Parameters)
{
	if (!GEngine) { AddError(TEXT("Engine unavailable")); return false; }
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("World"), World)) { return false; }
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeNewWorld(UWorld::InitializationValues().AllowAudioPlayback(false)
		.RequiresHitProxies(false).CreatePhysicsScene(true).CreateNavigation(false)
		.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false));
	const FString Slot = TEXT("SovAutomationPlayerSlot_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	ON_SCOPE_EXIT
	{
		USovTravelOwnerFenceTestSave::OnTravelSerialization = {};
		UGameplayStatics::DeleteGameInSlot(Slot, 0);
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	};
	auto* PC = World->SpawnActor<ASovHandoffRuntimeTestController>();
	auto* Pawn = World->SpawnActor<ASovHandoffRuntimeTestPawn>();
	auto* PS = World->SpawnActor<ASovPlayerState>();
	auto* Save = World->GetSubsystem<UNarrativeSaveSubsystem>();
	if (!PC || !Pawn || !PS || !Save) { AddError(TEXT("Save fixture creation failed")); return false; }
	auto* Definition = NewObject<UPlayerDefinition>(PC); PC->KeepAlive.Add(Definition);
	if (!TestTrue(TEXT("Prepare managed fixture"), Pawn->PrepareCampaignInitialization(Definition))) { return false; }
	PC->SetTestPlayerState(PS); PC->Possess(Pawn);
	if (!TestTrue(TEXT("Initialize actual components"), Pawn->StageTestReadiness(PS, true))
		|| !TestTrue(TEXT("Publish fixture readiness"), Pawn->CompleteCampaignDataInitialization(false))) { return false; }
	if (!TestTrue(TEXT("Create independent live world save"), Save->UpdateSaveObject(true))) { return false; }
	UNarrativeSave* WorldSave = Save->GetSaveObject();
	WorldSave->LevelName = TEXT("LiveWorldMustRemain");
	auto* Seed = Cast<USovTravelOwnerFenceTestSave>(UGameplayStatics::CreateSaveGameObject(USovTravelOwnerFenceTestSave::StaticClass()));
	if (!TestNotNull(TEXT("Configured save subclass"), Seed)) { return false; }
	Seed->CharacterCreatorUsername = TEXT("ExistingCreatorName");
	Seed->LevelName = TEXT("ExistingSlotMetadata");
	if (!TestTrue(TEXT("Seed unique temporary slot"), UGameplayStatics::SaveGameToSlot(Seed, Slot, 0))) { return false; }
	if (!TestTrue(TEXT("Update player records in existing slot"), Save->CreatePlayerOnlySaveInSlot(PC, Slot))) { return false; }
	auto* Loaded = Cast<UNarrativeSaveWithCreatorData>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
	if (!TestNotNull(TEXT("Save subclass survives player-only update"), Loaded)) { return false; }
	TestEqual(TEXT("Existing character creator data survives"), Loaded->CharacterCreatorUsername, Seed->CharacterCreatorUsername);
	TestEqual(TEXT("Other existing slot metadata survives"), Loaded->LevelName, Seed->LevelName);
	TestTrue(TEXT("New player records were written"), Loaded->PlayerData.IsValid()
		&& Loaded->PlayerData.PlayerStateData.IsValid() && Loaded->PlayerData.ControllerData.IsValid());
	TestEqual(TEXT("Player record belongs to current pawn"), Loaded->PlayerData.PawnData.ActorName, Pawn->GetFName());
	TestTrue(TEXT("Player-only save never replaces the live world save"), Save->GetSaveObject() == WorldSave);
	TestEqual(TEXT("Live world metadata is untouched"), WorldSave->LevelName, FString(TEXT("LiveWorldMustRemain")));
	TestFalse(TEXT("Unresolved platform user cannot write a travel record"), Save->CreatePlayerOnlySaveInSlot(PC, Slot, INDEX_NONE));
	FNarrativeSavePlayer ReadBack = Loaded->PlayerData;
	TestFalse(TEXT("Unresolved platform user cannot read a travel record"), Save->ReadPlayerOnlySave(Slot, ReadBack, INDEX_NONE));
	TestFalse(TEXT("A failed read cannot leave another user's stale records in the output"), ReadBack.IsValid());
	TestTrue(TEXT("Explicit resolved user reads the existing record"), Save->ReadPlayerOnlySave(Slot, ReadBack, 0));
	TestEqual(TEXT("Explicit user read retains the pawn identity"), ReadBack.PawnData.ActorName, Pawn->GetFName());
	TArray<uint8> BeforeRevocation, AfterRevocation;
	if (!TestTrue(TEXT("Capture exact existing travel slot bytes"), UGameplayStatics::LoadDataFromSlot(BeforeRevocation, Slot, 0))) { return false; }
	bool bOwnerCurrent = true; bool bPrepareCallbackRan = false;
	PC->OnPrepareTravelSave = [&bOwnerCurrent, &bPrepareCallbackRan]() { bPrepareCallbackRan = true; bOwnerCurrent = false; };
	const bool bRevokedWrite = Save->CreatePlayerOnlySaveInSlot(PC, Slot, 0, [&bOwnerCurrent]() { return bOwnerCurrent; });
	PC->OnPrepareTravelSave = {};
	TestTrue(TEXT("Real PrepareForSave callback revoked the owner"), bPrepareCallbackRan);
	TestFalse(TEXT("Capture-time owner revocation prevents travel write"), bRevokedWrite);
	TestTrue(TEXT("Previous travel slot remains readable"), UGameplayStatics::LoadDataFromSlot(AfterRevocation, Slot, 0));
	TestTrue(TEXT("Owner revocation preserves the existing bytes exactly"), BeforeRevocation == AfterRevocation);
	bOwnerCurrent = true; bool bSerializeCallbackRan = false;
	USovTravelOwnerFenceTestSave::OnTravelSerialization = [&bOwnerCurrent, &bSerializeCallbackRan](bool bSaving)
	{ if (bSaving) { bSerializeCallbackRan = true; bOwnerCurrent = false; } };
	const bool bSerializedAfterRevocation = Save->CreatePlayerOnlySaveInSlot(PC, Slot, 0, [&bOwnerCurrent]() { return bOwnerCurrent; });
	USovTravelOwnerFenceTestSave::OnTravelSerialization = {};
	TestTrue(TEXT("Real SaveGame serialization revoked the owner"), bSerializeCallbackRan);
	TestFalse(TEXT("Serialization-time owner revocation prevents platform write"), bSerializedAfterRevocation);
	TestTrue(TEXT("Slot remains readable after serialization veto"), UGameplayStatics::LoadDataFromSlot(AfterRevocation, Slot, 0));
	TestTrue(TEXT("Serialization-time owner revocation preserves exact bytes"), BeforeRevocation == AfterRevocation);
	TestFalse(TEXT("Unowned read refuses before publishing prior records"), Save->ReadPlayerOnlySave(Slot, ReadBack, 0, []() { return false; }));
	TestFalse(TEXT("Unowned read clears prior records"), ReadBack.IsValid());
	bOwnerCurrent = true; bool bDeserializeCallbackRan = false;
	USovTravelOwnerFenceTestSave::OnTravelSerialization = [&bOwnerCurrent, &bDeserializeCallbackRan](bool bSaving)
	{ if (!bSaving) { bDeserializeCallbackRan = true; bOwnerCurrent = false; } };
	const bool bReadAfterRevocation = Save->ReadPlayerOnlySave(Slot, ReadBack, 0, [&bOwnerCurrent]() { return bOwnerCurrent; });
	USovTravelOwnerFenceTestSave::OnTravelSerialization = {};
	TestTrue(TEXT("Real SaveGame deserialization revoked the owner"), bDeserializeCallbackRan);
	TestFalse(TEXT("Owner lost across loading refuses the completed read"), bReadAfterRevocation);
	TestFalse(TEXT("A read crossing owner loss exposes no records"), ReadBack.IsValid());
	return true;
}
#endif
