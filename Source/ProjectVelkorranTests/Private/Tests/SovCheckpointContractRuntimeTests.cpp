// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovProtagonistPartitionTestSupport.h"
#include "Campaign/SovEncounterSnapshotLibrary.h"
#include "Save/SovCampaignSaveGame.h"
#include "Save/SovSaveSubsystem.h"

#if WITH_AUTOMATION_TESTS
/**
 * C3 checkpoint contract. Writes go through the production capture, autosave tick and two-bank
 * writer; reads go through the production best-bank reader and Narrative decoder; the decoded
 * player records are staged by the controller exactly as the campaign game mode stages them.
 * Only map travel is not exercised.
 */
struct FSovCheckpointContractTestAccess
{
	static void UseStorage(USovSaveSubsystem& S, TUniquePtr<ISovSaveStorage> Storage)
	{ S.AccountNamespace = TEXT("c3-checkpoint-contract"); S.UserIndex = 0; S.Storage = MoveTemp(Storage); }
	static ESovSaveResult Capture(USovSaveSubsystem& S, ESovSaveSlotKind Kind, int32 Index, FString& Error)
	{ return S.CaptureAndWrite(Kind, Index, NAME_None, Error); }
	static void Tick(USovSaveSubsystem& S) { S.Tick(0.1f); }
	static int32 QueuedAutosaves(const USovSaveSubsystem& S) { return S.PendingAutosaves.Num(); }
	static USovCampaignSaveGame* ReadBest(USovSaveSubsystem& S, ESovSaveSlotKind Kind, int32 Index, bool& bDamaged, FString& Error)
	{ int32 Bank = INDEX_NONE; return S.ReadBest(Kind, Index, Bank, bDamaged, Error); }
	static bool Validate(USovSaveSubsystem& S, USovCampaignSaveGame* Save, FString& Error) { return S.ValidateEnvelope(Save, false, Error); }
	static UNarrativeSave* Decode(USovSaveSubsystem& S, USovCampaignSaveGame* Save, FString& Error) { return S.DecodeNarrative(Save, Error); }
	static FString BankName(const USovSaveSubsystem& S, ESovSaveSlotKind Kind, int32 Index, int32 Bank) { return S.BankName(Kind, Index, Bank); }
	static int32 SchemaVersion(const USovCampaignStateComponent& State) { return State.SavedSchemaVersion; }
	/** Schema 1 as the objective-lifecycle migration defines it: completed beats only, no lifecycle journal. */
	static void MakeSchemaOne(USovCampaignStateComponent& State)
	{
		State.SavedSchemaVersion = 1; State.ObjectiveJournal.Reset();
		for (auto& Pair : State.Missions) { Pair.Value.ObjectiveStates.Reset(); Pair.Value.SelectedChoices.Reset(); }
	}
};

namespace
{
	/** Physical bytes outlive any subsystem instance, as a disk does across a process restart. */
	class FSovCheckpointContractDisk final : public ISovSaveStorage
	{
	public:
		explicit FSovCheckpointContractDisk(TMap<FString, TArray<uint8>>& InBytes) : Bytes(InBytes) {}
		TMap<FString, TArray<uint8>>& Bytes;
		bool bDenyWrite = false;
		bool bTearWrite = false;
		bool bAcknowledgeTornWrite = false;
		static FString Key(const FString& Slot, int32 User) { return FString::FromInt(User) + TEXT(":") + Slot; }
		virtual bool Read(const FString& Slot, int32 User, TArray<uint8>& Out) override
		{ const auto* Found = Bytes.Find(Key(Slot, User)); if (!Found) { return false; } Out = *Found; return true; }
		virtual bool Write(const FString& Slot, int32 User, const TArray<uint8>& In) override
		{
			if (bDenyWrite) { return false; }
			TArray<uint8>& Stored = Bytes.FindOrAdd(Key(Slot, User)); Stored = In;
			if (bTearWrite) { Stored.SetNum(Stored.Num() / 2); return bAcknowledgeTornWrite; }
			return true;
		}
		virtual bool Exists(const FString& Slot, int32 User) override { return Bytes.Contains(Key(Slot, User)); }
	};

	/** A production save subsystem over a retained disk; constructing another one models a restart. */
	struct FSovCheckpointContractOwner
	{
		TStrongObjectPtr<USovSaveSubsystem> Subsystem;
		FSovCheckpointContractDisk* Disk = nullptr;
		FSovCheckpointContractOwner(UGameInstance* Instance, TMap<FString, TArray<uint8>>& Bytes)
		{
			Subsystem.Reset(NewObject<USovSaveSubsystem>(Instance));
			TUniquePtr<FSovCheckpointContractDisk> Storage = MakeUnique<FSovCheckpointContractDisk>(Bytes);
			Disk = Storage.Get();
			FSovCheckpointContractTestAccess::UseStorage(*Subsystem, MoveTemp(Storage));
		}
		FString Key(ESovSaveSlotKind Kind, int32 Index, int32 Bank) const
		{ return FSovCheckpointContractDisk::Key(FSovCheckpointContractTestAccess::BankName(*Subsystem, Kind, Index, Bank), 0); }
		/** The production read path up to the records the campaign game mode hands to the controller. */
		bool Load(ESovSaveSlotKind Kind, int32 Index, FNarrativeSavePlayer& OutRecords, bool& bDamaged, FString& Error, int64* OutGeneration = nullptr)
		{
			USovCampaignSaveGame* Best = FSovCheckpointContractTestAccess::ReadBest(*Subsystem, Kind, Index, bDamaged, Error);
			if (!Best) { return false; }
			if (OutGeneration) { *OutGeneration = Best->Header.Generation; }
			UNarrativeSave* Decoded = FSovCheckpointContractTestAccess::Decode(*Subsystem, Best, Error);
			if (!Decoded) { return false; }
			OutRecords = Decoded->PlayerData;
			return OutRecords.IsValid();
		}
	};

	FName SovCampaignStateRecordName() { return GetDefault<ASovPlayerController>()->GetCampaignState()->GetFName(); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCheckpointRollingAutosaveTest, "ProjectVelkorran.Campaign.CheckpointContract.RollingAutosavesReplaceOldestAcrossRestart",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCheckpointRollingAutosaveTest::RunTest(const FString& Parameters)
{
	using namespace SovPartitionTest;
	using FAccess = FSovCheckpointContractTestAccess;
	FPartitionCampaign C;
	TMap<FString, TArray<uint8>> Disk;
	FPartitionWorld W(true); if (!W.IsValid()) { AddError(TEXT("Local save-owner world failed")); return false; }
	if (!TestNotNull(TEXT("Tarrik enters"), W.Enter(C.Missions[0], nullptr))) { AddError(W.Error); return false; }
	if (!TestTrue(TEXT("Protagonist is grounded for save admission"), W.Ground())) { return false; }

	const auto AutosaveThroughTick = [&W](FSovCheckpointContractOwner& Owner, int32 Sequence)
	{
		W.Current()->GetInventoryComponent()->SetCurrency(100 + Sequence);
		Owner.Subsystem->QueueAutosave(ESovSaveBoundary::ExplicitCheckpoint, FName(*FString::Printf(TEXT("Rotation%d"), Sequence)));
		FAccess::Tick(*Owner.Subsystem);
		return FAccess::QueuedAutosaves(*Owner.Subsystem) == 0;
	};
	const auto SlotContents = [](FSovCheckpointContractOwner& Owner, int32 Slot, int64& OutGeneration, bool& bOutDamaged)
	{
		FString Error; bOutDamaged = false;
		const USovCampaignSaveGame* Save = FAccess::ReadBest(*Owner.Subsystem, ESovSaveSlotKind::Auto, Slot, bOutDamaged, Error);
		OutGeneration = Save ? Save->Header.Generation : 0;
		return Save ? Save->Header.BoundaryId : NAME_None;
	};

	{
		FSovCheckpointContractOwner Owner(W.Instance.Get(), Disk);
		FString Error;
		if (!TestTrue(TEXT("The real admission gate accepts this safe state"), Owner.Subsystem->CanCapture(Error))) { AddError(Error); return false; }
		for (int32 Sequence = 0; Sequence < 4; ++Sequence)
		{
			if (!TestTrue(FString::Printf(TEXT("Autosave %d commits through the queued boundary and tick"), Sequence), AutosaveThroughTick(Owner, Sequence))) { return false; }
		}
		int64 G0 = 0, G1 = 0, G2 = 0; bool bDamaged = false;
		TestEqual(TEXT("The fourth autosave replaced the oldest slot"), SlotContents(Owner, 0, G0, bDamaged), FName(TEXT("Rotation3")));
		TestEqual(TEXT("The second slot was not replaced"), SlotContents(Owner, 1, G1, bDamaged), FName(TEXT("Rotation1")));
		TestEqual(TEXT("The third slot was not replaced"), SlotContents(Owner, 2, G2, bDamaged), FName(TEXT("Rotation2")));
		TestTrue(TEXT("Generations form one rolling sequence across the three slots"), G0 == 4 && G1 == 2 && G2 == 3);
		TestTrue(TEXT("The replaced slot keeps its previous generation in its other bank"),
			Disk.Contains(Owner.Key(ESovSaveSlotKind::Auto, 0, 0)) && Disk.Contains(Owner.Key(ESovSaveSlotKind::Auto, 0, 1)));
	}
	{
		// A process restart: a new subsystem sees only the physical bytes.
		FSovCheckpointContractOwner Restarted(W.Instance.Get(), Disk);
		if (!TestTrue(TEXT("An autosave after restart commits"), AutosaveThroughTick(Restarted, 4))) { return false; }
		int64 Generation = 0; bool bDamaged = false;
		TestEqual(TEXT("After restart the oldest remaining slot is replaced"), SlotContents(Restarted, 1, Generation, bDamaged), FName(TEXT("Rotation4")));
		TestEqual(TEXT("Generation continues from disk, not from memory"), Generation, int64(5));

		// Corrupted newest autosave: the next rotation target is slot 2 (generation 3), and its write tears.
		AddExpectedError(TEXT("Failed loading tagged"), EAutomationExpectedErrorFlags::Contains, 0);
		Restarted.Disk->bTearWrite = true;
		TestFalse(TEXT("A torn autosave never reports success"), AutosaveThroughTick(Restarted, 5));
		TestTrue(TEXT("The failed write waits for an explicit decision"), Restarted.Subsystem->IsAwaitingFailureDecision());
		TestEqual(TEXT("The torn slot still yields its last known-good generation"), SlotContents(Restarted, 2, Generation, bDamaged), FName(TEXT("Rotation2")));
		TestTrue(TEXT("That generation is 3 and the damage is reported"), Generation == 3 && bDamaged);
		int64 Newest = 0; FName NewestBoundary;
		for (const FSovSaveSlotHeader& Header : Restarted.Subsystem->ListSlots())
		{
			if (Header.Kind == ESovSaveSlotKind::Auto && Header.Generation > Newest) { Newest = Header.Generation; NewestBoundary = Header.BoundaryId; }
		}
		TestTrue(TEXT("The last known-good autosave offered is the newest verified generation"), Newest == 5 && NewestBoundary == FName(TEXT("Rotation4")));
		const FString TornKey = Restarted.Key(ESovSaveSlotKind::Auto, 2, 1);
		const TArray<uint8> TornBytes = Disk.FindRef(TornKey);

		// Storage recovers; the explicit retry commits the retained snapshot and preserves the torn bytes for support.
		Restarted.Disk->bTearWrite = false;
		FString Error;
		TestEqual(TEXT("An explicit retry commits the retained autosave"), Restarted.Subsystem->RetryFailedWrite(Error), ESovSaveResult::Success);
		TestEqual(TEXT("The retried autosave now occupies slot 2"), SlotContents(Restarted, 2, Generation, bDamaged), FName(TEXT("Rotation5")));
		TestTrue(TEXT("The retry advanced to generation 6 without damage"), Generation == 6 && !bDamaged);
		bool bPreserved = false;
		for (const auto& Pair : Disk) { bPreserved |= Pair.Key.Contains(TEXT("_Recovery_")) && Pair.Value == TornBytes; }
		TestTrue(TEXT("The torn bytes were preserved for support before their bank was reused"), bPreserved && !TornBytes.IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCheckpointInterruptedWriteTest, "ProjectVelkorran.Campaign.CheckpointContract.InterruptedWriteRecoversLastGoodCampaign",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCheckpointInterruptedWriteTest::RunTest(const FString& Parameters)
{
	using namespace SovPartitionTest;
	using FAccess = FSovCheckpointContractTestAccess;
	FPartitionCampaign C;
	TMap<FString, TArray<uint8>> Disk;
	{
		FPartitionWorld W(true); if (!W.IsValid()) { AddError(TEXT("Local save-owner world failed")); return false; }
		if (!PlayBothProtagonists(*this, W, C) || !TestTrue(TEXT("Grounded for save admission"), W.Ground())) { return false; }
		FSovCheckpointContractOwner Owner(W.Instance.Get(), Disk);
		FString Error;
		if (!TestEqual(TEXT("Save A commits"), FAccess::Capture(*Owner.Subsystem, ESovSaveSlotKind::Manual, 0, Error), ESovSaveResult::Success)) { AddError(Error); return false; }
		const TMap<FString, TArray<uint8>> AfterA = Disk;
		const FString BankA = Owner.Key(ESovSaveSlotKind::Manual, 0, 0);

		W.Current()->GetInventoryComponent()->TryAddItemFromClass(USovPartitionSeleneKit::StaticClass(), 5);
		W.Current()->GetInventoryComponent()->SetCurrency(107);
		Owner.Disk->bDenyWrite = true;
		TestEqual(TEXT("A denied write of Save B is never success"), FAccess::Capture(*Owner.Subsystem, ESovSaveSlotKind::Manual, 0, Error), ESovSaveResult::WriteFailed);
		TestTrue(TEXT("A denied write leaves the disk exactly as Save A left it"), Disk.OrderIndependentCompareEqual(AfterA));
		Owner.Subsystem->AcknowledgeSaveFailure();

		// Storage that claims success for a half-written file: an interrupted write the platform did not report.
		Owner.Disk->bDenyWrite = false; Owner.Disk->bTearWrite = true; Owner.Disk->bAcknowledgeTornWrite = true;
		AddExpectedError(TEXT("Failed loading tagged"), EAutomationExpectedErrorFlags::Contains, 0);
		TestEqual(TEXT("A falsely acknowledged torn write fails readback"), FAccess::Capture(*Owner.Subsystem, ESovSaveSlotKind::Manual, 0, Error), ESovSaveResult::ReadbackFailed);
		TestTrue(TEXT("Save A's bank is byte-for-byte untouched by the interrupted write"), Disk.FindRef(BankA) == AfterA.FindRef(BankA));
		Owner.Subsystem->AcknowledgeSaveFailure();
	}

	FNarrativeSavePlayer Records;
	{
		// Process restart: a new game instance and subsystem over the same physical bytes.
		TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
		FSovCheckpointContractOwner Owner(Instance.Get(), Disk);
		bool bDamaged = false; FString Error; int64 Generation = 0;
		if (!TestTrue(TEXT("The loader recovers and decodes Save A"), Owner.Load(ESovSaveSlotKind::Manual, 0, Records, bDamaged, Error, &Generation))) { AddError(Error); return false; }
		TestTrue(TEXT("The torn newer bank is reported rather than silently used"), bDamaged);
		TestEqual(TEXT("The recovered generation is Save A's"), Generation, int64(1));
	}
	{
		FPartitionWorld W; if (!W.IsValid()) { AddError(TEXT("World creation failed")); return false; }
		if (!TestNotNull(TEXT("Recovered Save A loads"), W.Enter(C.Missions[1], &Records))) { AddError(W.Error); return false; }
		ExpectSelene(*this, W, TEXT("Recovered Save A"), { 2, 7, 3, 12.f });
		ExpectCampaignBoundary(*this, W, C, TEXT("Recovered Save A"), true);
		if (!TestNotNull(TEXT("Handoff from recovered Save A"), W.Handoff(C.Missions[2]))) { AddError(W.Error); return false; }
		ExpectTarrik(*this, W, TEXT("Tarrik from recovered Save A"), { 3, 40, 5, 33.f });
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCheckpointMigrationTest, "ProjectVelkorran.Campaign.CheckpointContract.SchemaOneCampaignMigratesThroughLoaderDeterministically",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCheckpointMigrationTest::RunTest(const FString& Parameters)
{
	using namespace SovPartitionTest;
	using FAccess = FSovCheckpointContractTestAccess;
	FPartitionCampaign C;
	TMap<FString, TArray<uint8>> Disk;
	{
		FPartitionWorld W(true); if (!W.IsValid()) { AddError(TEXT("Local save-owner world failed")); return false; }
		if (!PlayBothProtagonists(*this, W, C) || !TestTrue(TEXT("Grounded for save admission"), W.Ground())) { return false; }
		FAccess::MakeSchemaOne(*W.Campaign());
		TestEqual(TEXT("The saved campaign state is the schema-1 shape"), FAccess::SchemaVersion(*W.Campaign()), 1);
		FSovCheckpointContractOwner Owner(W.Instance.Get(), Disk);
		FString Error;
		if (!TestEqual(TEXT("A schema-1 campaign save commits"), FAccess::Capture(*Owner.Subsystem, ESovSaveSlotKind::Manual, 0, Error), ESovSaveResult::Success)) { AddError(Error); return false; }
	}

	TArray<uint8> MigratedState[2];
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		const FString When = FString::Printf(TEXT("Migration pass %d"), Pass);
		FNarrativeSavePlayer Records;
		{
			TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
			FSovCheckpointContractOwner Owner(Instance.Get(), Disk);
			bool bDamaged = false; FString Error;
			if (!TestTrue(When + TEXT(": the schema-1 save passes preflight and decodes"), Owner.Load(ESovSaveSlotKind::Manual, 0, Records, bDamaged, Error))) { AddError(Error); return false; }
			TestFalse(When + TEXT(": the envelope is not damaged"), bDamaged);
		}
		FPartitionWorld W(true); if (!W.IsValid()) { AddError(TEXT("Local save-owner world failed")); return false; }
		if (!TestNotNull(When + TEXT(": the migrated campaign loads"), W.Enter(C.Missions[1], &Records))) { AddError(W.Error); return false; }
		TestEqual(When + TEXT(": campaign state advanced to schema 2"), FAccess::SchemaVersion(*W.Campaign()), 2);
		TestTrue(When + TEXT(": migrated campaign state validates"), W.Campaign()->IsStateValid());
		TestEqual(When + TEXT(": Tarrik's completed beat becomes a succeeded objective"),
			W.Campaign()->GetObjectiveState(C.Missions[0]->MissionId, TEXT("Done")), ESovObjectiveState::Succeeded);
		TestEqual(When + TEXT(": Selene's completed beat becomes a succeeded objective"),
			W.Campaign()->GetObjectiveState(C.Missions[1]->MissionId, TEXT("Done")), ESovObjectiveState::Succeeded);
		TestTrue(When + TEXT(": migration invents no lifecycle journal"), W.Campaign()->GetObjectiveJournal().IsEmpty());
		ExpectSelene(*this, W, When, { 2, 7, 3, 12.f });
		ExpectCampaignBoundary(*this, W, C, When, true);

		FNarrativeSaveComponent Record;
		TestTrue(When + TEXT(": migrated campaign state re-serializes"), USovEncounterSnapshotLibrary::CaptureComponent(W.Campaign(), Record));
		MigratedState[Pass] = Record.ByteData;

		// TDD 15.9: the save header carries migration history. Saving the migrated campaign again must record it.
		TMap<FString, TArray<uint8>> Resaved;
		{
			TestTrue(When + TEXT(": grounded for re-save"), W.Ground());
			FSovCheckpointContractOwner Owner(W.Instance.Get(), Resaved);
			FString Error; bool bDamaged = false;
			if (TestEqual(When + TEXT(": the migrated campaign saves again"), FAccess::Capture(*Owner.Subsystem, ESovSaveSlotKind::Manual, 0, Error), ESovSaveResult::Success))
			{
				const USovCampaignSaveGame* Again = FAccess::ReadBest(*Owner.Subsystem, ESovSaveSlotKind::Manual, 0, bDamaged, Error);
				TestTrue(When + TEXT(": the re-saved header records the campaign-state migration"),
					Again && Again->Header.MigrationHistory.Contains(TEXT("CampaignState 1->2")));
			}
			else { AddError(Error); }
		}

		if (!TestNotNull(When + TEXT(": handoff after migration"), W.Handoff(C.Missions[2]))) { AddError(W.Error); return false; }
		ExpectTarrik(*this, W, When + TEXT(" then Tarrik"), { 3, 40, 5, 33.f });
	}
	TestTrue(TEXT("Migrating the same saved bytes twice yields byte-identical campaign state"),
		!MigratedState[0].IsEmpty() && MigratedState[0] == MigratedState[1]);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCheckpointMigrationMissingRecordTest, "ProjectVelkorran.Campaign.CheckpointContract.MissingActiveRecordStaysRejectedAfterMigration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCheckpointMigrationMissingRecordTest::RunTest(const FString& Parameters)
{
	using namespace SovPartitionTest;
	using FAccess = FSovCheckpointContractTestAccess;
	FPartitionCampaign C;
	const auto& Tags = FSovGameplayTags::Get();
	TMap<FString, TArray<uint8>> Disk;
	{
		FPartitionWorld W(true); if (!W.IsValid()) { AddError(TEXT("Local save-owner world failed")); return false; }
		if (!W.Enter(C.Missions[0], nullptr)) { AddError(W.Error); return false; }
		W.Current()->GetInventoryComponent()->TryAddItemFromClass(USovPartitionTarrikKit::StaticClass(), 3);
		W.Current()->GetInventoryComponent()->SetCurrency(40);
		W.ClaimTechniqueReward(TEXT("Partition.Tarrik"), 5, Tags.Character_Player_Tarrik);
		W.CompleteActiveMission();
		if (!TestTrue(TEXT("Grounded for save admission"), W.Ground())) { return false; }
		FSovCheckpointContractOwner Owner(W.Instance.Get(), Disk);
		FString Error;
		if (!TestEqual(TEXT("Tarrik-only save commits"), FAccess::Capture(*Owner.Subsystem, ESovSaveSlotKind::Manual, 0, Error), ESovSaveResult::Success)) { AddError(Error); return false; }
		if (!W.Handoff(C.Missions[1]) || !W.Ground()) { AddError(W.Error); return false; }
		FAccess::MakeSchemaOne(*W.Campaign());
		if (!TestEqual(TEXT("Selene-active schema-1 save commits"), FAccess::Capture(*Owner.Subsystem, ESovSaveSlotKind::Manual, 1, Error), ESovSaveResult::Success)) { AddError(Error); return false; }
	}

	FNarrativeSavePlayer TarrikOnly, SeleneActive;
	{
		TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
		FSovCheckpointContractOwner Owner(Instance.Get(), Disk);
		bool bDamaged = false; FString Error;
		if (!TestTrue(TEXT("Tarrik-only save decodes"), Owner.Load(ESovSaveSlotKind::Manual, 0, TarrikOnly, bDamaged, Error))
			|| !TestTrue(TEXT("Selene-active schema-1 save decodes"), Owner.Load(ESovSaveSlotKind::Manual, 1, SeleneActive, bDamaged, Error))) { AddError(Error); return false; }
	}
	FNarrativeSavePlayer Damaged = SeleneActive;
	Damaged.PlayerStateData = TarrikOnly.PlayerStateData;
	const FNarrativeSaveComponent* CampaignRecord = Damaged.ControllerData.SavedComponents.FindByPredicate(
		[](const FNarrativeSaveComponent& Item) { return Item.ComponentName == SovCampaignStateRecordName(); });
	FString Error;
	// The migration itself succeeds, so any refusal below is the missing record and not a failed migration.
	TestTrue(TEXT("The schema-1 campaign record migrates and validates on its own"),
		CampaignRecord && USovCampaignStateComponent::ValidateSerializedSave(CampaignRecord->ByteData, Error));
	{
		FPartitionWorld W; if (!W.IsValid()) { AddError(TEXT("World creation failed")); return false; }
		AddExpectedError(TEXT("lacks a matching protagonist snapshot"), EAutomationExpectedErrorFlags::Contains, 1);
		const ASovPlayerCharacterBase* Selene = W.Enter(C.Missions[1], &Damaged);
		if (!TestNotNull(TEXT("The Selene pawn is created for the attempted load"), Selene)) { AddError(W.Error); return false; }
		TestEqual(TEXT("Migration does not make the missing record loadable"), W.PC->GetCampaignTransitionState(), ESovCampaignTransitionState::Failed);
		TestFalse(TEXT("Selene never becomes ready"), Selene->IsCharacterReady());
		TestEqual(TEXT("Selene does not borrow Tarrik's kit"), CountItems(Selene, USovPartitionTarrikKit::StaticClass()), 0);
		TestEqual(TEXT("Selene does not borrow Tarrik's currency"), Currency(Selene), 0);
		TestEqual(TEXT("Selene does not borrow Tarrik's Technique points"), W.Techniques()->GetAvailableTechniquePoints(), 0);
		FSovProtagonistSnapshot Borrowed;
		TestFalse(TEXT("No Selene record was fabricated from Tarrik's"), W.PS->FindProtagonistSnapshot(Tags.Character_Player_Selene, Borrowed));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCheckpointCorruptSaveTest, "ProjectVelkorran.Campaign.CheckpointContract.CorruptOrIncompleteCampaignSaveIsRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCheckpointCorruptSaveTest::RunTest(const FString& Parameters)
{
	using namespace SovPartitionTest;
	using FAccess = FSovCheckpointContractTestAccess;
	FPartitionCampaign C;
	TMap<FString, TArray<uint8>> Disk;
	{
		FPartitionWorld W(true); if (!W.IsValid()) { AddError(TEXT("Local save-owner world failed")); return false; }
		if (!PlayBothProtagonists(*this, W, C) || !TestTrue(TEXT("Grounded for save admission"), W.Ground())) { return false; }
		FSovCheckpointContractOwner Owner(W.Instance.Get(), Disk);
		FString Error;
		if (!TestEqual(TEXT("Generation 1 commits"), FAccess::Capture(*Owner.Subsystem, ESovSaveSlotKind::Manual, 0, Error), ESovSaveResult::Success)) { AddError(Error); return false; }
		W.Current()->GetInventoryComponent()->SetCurrency(107);
		if (!TestEqual(TEXT("Generation 2 commits"), FAccess::Capture(*Owner.Subsystem, ESovSaveSlotKind::Manual, 0, Error), ESovSaveResult::Success)) { AddError(Error); return false; }
	}

	TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
	FSovCheckpointContractOwner Owner(Instance.Get(), Disk);
	const FString KeyA = Owner.Key(ESovSaveSlotKind::Manual, 0, 0), KeyB = Owner.Key(ESovSaveSlotKind::Manual, 0, 1);
	const TArray<uint8> GoodA = Disk.FindRef(KeyA), GoodB = Disk.FindRef(KeyB);
	if (!TestTrue(TEXT("Both banks exist"), !GoodA.IsEmpty() && !GoodB.IsEmpty())) { return false; }
	AddExpectedError(TEXT("Failed loading tagged"), EAutomationExpectedErrorFlags::Contains, 0);

	struct FCorruption { const TCHAR* Name; TFunction<void(TArray<uint8>&)> Apply; };
	const TArray<FCorruption> Corruptions = {
		{ TEXT("Truncated newest bank"), [](TArray<uint8>& Bytes) { Bytes.SetNum(Bytes.Num() / 2); } },
		{ TEXT("Empty newest bank"), [](TArray<uint8>& Bytes) { Bytes.Reset(); } },
		{ TEXT("Zeroed newest bank"), [](TArray<uint8>& Bytes) { FMemory::Memzero(Bytes.GetData(), Bytes.Num()); } },
		{ TEXT("Bit flip inside the newest payload"), [](TArray<uint8>& Bytes) { Bytes[Bytes.Num() / 2] ^= 0x40; } },
	};
	for (const FCorruption& Corruption : Corruptions)
	{
		const FString When = Corruption.Name;
		Disk.FindOrAdd(KeyB) = GoodB;
		Corruption.Apply(Disk.FindChecked(KeyB));
		const TMap<FString, TArray<uint8>> Before = Disk;
		bool bDamaged = false; FString Error; int64 Generation = 0;
		FNarrativeSavePlayer Records;
		TestTrue(When + TEXT(": the previous generation still loads"), Owner.Load(ESovSaveSlotKind::Manual, 0, Records, bDamaged, Error, &Generation));
		TestTrue(When + TEXT(": the damaged newest bank is rejected and reported"), bDamaged && Generation == 1);
		TestTrue(When + TEXT(": reading preserves every byte on disk"), Disk.OrderIndependentCompareEqual(Before));
	}
	Disk.FindOrAdd(KeyB) = GoodB;

	// A well-formed, correctly checksummed envelope whose Narrative payload is incomplete.
	TStrongObjectPtr<USovCampaignSaveGame> Incomplete(Cast<USovCampaignSaveGame>(UGameplayStatics::LoadGameFromMemory(GoodB)));
	if (!TestNotNull(TEXT("The newest envelope decodes for mutation"), Incomplete.Get())) { return false; }
	Incomplete->NarrativePayload.SetNum(Incomplete->NarrativePayload.Num() / 3);
	Incomplete->IntegrityChecksum = Incomplete->CalculateChecksum();
	FString Error;
	TestTrue(TEXT("The incomplete envelope passes integrity, isolating the payload check"), FAccess::Validate(*Owner.Subsystem, Incomplete.Get(), Error));
	TestNull(TEXT("An incomplete Narrative payload is rejected before any world is touched"), FAccess::Decode(*Owner.Subsystem, Incomplete.Get(), Error));

	// A complete payload that has lost its required canon-state record.
	TStrongObjectPtr<USovCampaignSaveGame> Canonless(Cast<USovCampaignSaveGame>(UGameplayStatics::LoadGameFromMemory(GoodB)));
	TStrongObjectPtr<UNarrativeSave> Payload(Cast<UNarrativeSave>(UGameplayStatics::LoadGameFromMemory(Canonless->NarrativePayload)));
	if (!TestNotNull(TEXT("The newest payload decodes for mutation"), Payload.Get())) { return false; }
	const int32 Removed = Payload->PlayerData.ControllerData.SavedComponents.RemoveAll(
		[](const FNarrativeSaveComponent& Item) { return Item.ComponentName == SovCampaignStateRecordName(); });
	TestEqual(TEXT("Exactly the canon-state record was removed"), Removed, 1);
	UGameplayStatics::SaveGameToMemory(Payload.Get(), Canonless->NarrativePayload);
	Canonless->IntegrityChecksum = Canonless->CalculateChecksum();
	TestTrue(TEXT("The canonless envelope passes integrity, isolating the canon check"), FAccess::Validate(*Owner.Subsystem, Canonless.Get(), Error));
	TestNull(TEXT("A payload without its canon-state record is rejected"), FAccess::Decode(*Owner.Subsystem, Canonless.Get(), Error));
	TestTrue(TEXT("Rejections never rewrite storage"), Disk.FindRef(KeyA) == GoodA && Disk.FindRef(KeyB) == GoodB);
	return true;
}
#endif
