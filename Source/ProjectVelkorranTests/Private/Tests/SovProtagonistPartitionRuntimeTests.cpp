// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovProtagonistPartitionTestSupport.h"
#include "GAS/NarrativeGameplayAbility.h"

#if WITH_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovProtagonistPartitionHandoffTest, "ProjectVelkorran.Campaign.ProtagonistPartition.RepeatedHandoffsKeepOwnedStateSeparate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovProtagonistPartitionHandoffTest::RunTest(const FString& Parameters)
{
	using namespace SovPartitionTest;
	FPartitionCampaign C;
	FPartitionWorld W; if (!W.IsValid()) { AddError(TEXT("World creation failed")); return false; }
	const auto& Tags = FSovGameplayTags::Get();

	if (!TestNotNull(TEXT("Tarrik enters through the managed campaign path"), W.Enter(C.Missions[0], nullptr))) { AddError(W.Error); return false; }
	ExpectTarrik(*this, W, TEXT("Tarrik first visit"), { 0, 0, 0, 10.f });
	auto* Tarrik = W.Current();
	Tarrik->GetInventoryComponent()->TryAddItemFromClass(USovPartitionTarrikKit::StaticClass(), 3);
	Tarrik->GetInventoryComponent()->SetCurrency(40);
	TestTrue(TEXT("Tarrik earns his own Technique reward"), W.ClaimTechniqueReward(TEXT("Partition.Tarrik"), 5, Tags.Character_Player_Tarrik));
	W.ASC()->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetEchoAttribute(), 33.f);
	const FGameplayAbilitySpecHandle TarrikGrant = Tarrik->AddAbility(UNarrativeGameplayAbility::StaticClass(), C.TarrikDefinition);
	const FActiveGameplayEffectHandle TarrikEffect = W.ASC()->ApplyGameplayEffectToSelf(
		GetDefault<USovPartitionTransientCombatEffect>(), 1.f, W.ASC()->MakeEffectContext());
	TestTrue(TEXT("Tarrik's transient combat state is live before handoff"), TarrikGrant.IsValid() && TarrikEffect.IsValid());
	TestTrue(TEXT("Tarrik completes his authored mission"), W.CompleteActiveMission());
	ExpectTarrik(*this, W, TEXT("Tarrik before first handoff"), { 3, 40, 5, 33.f });

	if (!TestNotNull(TEXT("Authored handoff to Selene"), W.Handoff(C.Missions[1]))) { AddError(W.Error); return false; }
	ExpectSelene(*this, W, TEXT("Selene first visit"), { 0, 0, 0, 20.f });
	TestNull(TEXT("Tarrik's granted ability spec does not survive into Selene's kit"), W.ASC()->FindAbilitySpecFromHandle(TarrikGrant));
	FGameplayEffectQuery TransientQuery; TransientQuery.EffectDefinition = USovPartitionTransientCombatEffect::StaticClass();
	TestEqual(TEXT("Tarrik's active combat effect does not survive into Selene"), W.ASC()->GetActiveEffects(TransientQuery).Num(), 0);
	TestTrue(TEXT("Selene's identity alone is on the shared ASC"), W.ASC()->HasMatchingGameplayTag(Tags.Character_Player_Selene)
		&& !W.ASC()->HasMatchingGameplayTag(Tags.Character_Player_Tarrik));
	AddInfo(FString::Printf(TEXT("Selene first-visit Health on the shared ASC with no authored DefaultAttributes effect: %.1f"),
		W.ASC()->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute())));
	ExpectCampaignBoundary(*this, W, C, TEXT("After first handoff"), false);

	auto* Selene = W.Current();
	Selene->GetInventoryComponent()->TryAddItemFromClass(USovPartitionSeleneKit::StaticClass(), 2);
	Selene->GetInventoryComponent()->SetCurrency(7);
	TestTrue(TEXT("Selene earns her own Technique reward"), W.ClaimTechniqueReward(TEXT("Partition.Selene"), 3, Tags.Character_Player_Selene));
	W.ASC()->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetEchoAttribute(), 12.f);
	TestTrue(TEXT("Selene completes her authored mission"), W.CompleteActiveMission());
	ExpectCampaignBoundary(*this, W, C, TEXT("After Selene learns"), true);

	// Three further round trips: owned records must neither merge nor overwrite each other.
	for (int32 Index = 2; Index < C.Missions.Num(); ++Index)
	{
		const FString When = FString::Printf(TEXT("Handoff %d"), Index);
		if (!TestNotNull(*(When + TEXT(" completes")), W.Handoff(C.Missions[Index]))) { AddError(W.Error); return false; }
		if (Index % 2 == 0) { ExpectTarrik(*this, W, When, { 3, 40, 5, 33.f }); }
		else { ExpectSelene(*this, W, When, { 2, 7, 3, 12.f }); }
		ExpectCampaignBoundary(*this, W, C, When, true);
		if (Index + 1 < C.Missions.Num() && !TestTrue(*(When + TEXT(" mission completes")), W.CompleteActiveMission())) { return false; }
	}
	TestNull(TEXT("A returning Tarrik does not regain a transient ability grant"), W.ASC()->FindAbilitySpecFromHandle(TarrikGrant));
	TestEqual(TEXT("A returning Tarrik does not regain a transient combat effect"), W.ASC()->GetActiveEffects(TransientQuery).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovProtagonistPartitionSaveLoadTest, "ProjectVelkorran.Campaign.ProtagonistPartition.SaveSwitchMutateReloadEitherProtagonist",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovProtagonistPartitionSaveLoadTest::RunTest(const FString& Parameters)
{
	using namespace SovPartitionTest;
	FPartitionCampaign C;
	TArray<uint8> SeleneActiveSave, TarrikActiveSave;
	{
		FPartitionWorld W; if (!W.IsValid()) { AddError(TEXT("World creation failed")); return false; }
		if (!PlayBothProtagonists(*this, W, C)) { return false; }
		if (!TestTrue(TEXT("Save while Selene is active"), W.WriteSave(SeleneActiveSave))) { return false; }

		// Mutate after the save, then switch and mutate the other protagonist too.
		W.Current()->GetInventoryComponent()->TryAddItemFromClass(USovPartitionSeleneKit::StaticClass(), 5);
		W.Current()->GetInventoryComponent()->SetCurrency(107);
		if (!W.Handoff(C.Missions[2])) { AddError(W.Error); return false; }
		ExpectTarrik(*this, W, TEXT("Tarrik after Selene's post-save mutation"), { 3, 40, 5, 33.f });
		W.Current()->GetInventoryComponent()->TryAddItemFromClass(USovPartitionTarrikKit::StaticClass(), 1);
		W.Current()->GetInventoryComponent()->SetCurrency(540);
		if (!TestTrue(TEXT("Save while Tarrik is active"), W.WriteSave(TarrikActiveSave))) { return false; }
		// A later live mutation that no reload may observe.
		W.Current()->GetInventoryComponent()->SetCurrency(9999);
	}

	FNarrativeSavePlayer SeleneRecords, TarrikRecords;
	if (!TestTrue(TEXT("Selene-active save decodes"), ReadSave(SeleneActiveSave, SeleneRecords))
		|| !TestTrue(TEXT("Tarrik-active save decodes"), ReadSave(TarrikActiveSave, TarrikRecords))) { return false; }
	{
		FPartitionWorld W; if (!W.IsValid()) { AddError(TEXT("World creation failed")); return false; }
		if (!TestNotNull(TEXT("Reload the Selene-active save"), W.Enter(C.Missions[1], &SeleneRecords))) { AddError(W.Error); return false; }
		ExpectSelene(*this, W, TEXT("Reloaded Selene"), { 2, 7, 3, 12.f });
		ExpectCampaignBoundary(*this, W, C, TEXT("Reloaded Selene"), true);
		if (!TestNotNull(TEXT("Handoff from the reloaded Selene"), W.Handoff(C.Missions[2]))) { AddError(W.Error); return false; }
		ExpectTarrik(*this, W, TEXT("Tarrik from the Selene-active save"), { 3, 40, 5, 33.f });
	}
	{
		FPartitionWorld W; if (!W.IsValid()) { AddError(TEXT("World creation failed")); return false; }
		if (!TestNotNull(TEXT("Reload the Tarrik-active save"), W.Enter(C.Missions[2], &TarrikRecords))) { AddError(W.Error); return false; }
		ExpectTarrik(*this, W, TEXT("Reloaded Tarrik"), { 4, 540, 5, 33.f });
		ExpectCampaignBoundary(*this, W, C, TEXT("Reloaded Tarrik"), true);
		TestTrue(TEXT("Tarrik completes his reloaded mission"), W.CompleteActiveMission());
		if (!TestNotNull(TEXT("Handoff from the reloaded Tarrik"), W.Handoff(C.Missions[3]))) { AddError(W.Error); return false; }
		ExpectSelene(*this, W, TEXT("Selene from the Tarrik-active save"), { 7, 107, 3, 12.f });
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovProtagonistPartitionMissingRecordTest, "ProjectVelkorran.Campaign.ProtagonistPartition.MissingProtagonistRecordFailsClosed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovProtagonistPartitionMissingRecordTest::RunTest(const FString& Parameters)
{
	using namespace SovPartitionTest;
	FPartitionCampaign C;
	const auto& Tags = FSovGameplayTags::Get();
	TArray<uint8> TarrikOnlySave, SeleneActiveSave;
	{
		FPartitionWorld W; if (!W.IsValid()) { AddError(TEXT("World creation failed")); return false; }
		if (!W.Enter(C.Missions[0], nullptr)) { AddError(W.Error); return false; }
		W.Current()->GetInventoryComponent()->TryAddItemFromClass(USovPartitionTarrikKit::StaticClass(), 3);
		W.Current()->GetInventoryComponent()->SetCurrency(40);
		W.ClaimTechniqueReward(TEXT("Partition.Tarrik"), 5, Tags.Character_Player_Tarrik);
		W.CompleteActiveMission();
		if (!TestTrue(TEXT("Save before Selene has ever been played"), W.WriteSave(TarrikOnlySave))) { return false; }
		FSovProtagonistSnapshot TarrikRecord;
		if (!TestTrue(TEXT("Tarrik's record exists"), W.PS->FindProtagonistSnapshot(Tags.Character_Player_Tarrik, TarrikRecord))) { return false; }
		TestFalse(TEXT("No Selene record exists yet"), W.PS->FindProtagonistSnapshot(Tags.Character_Player_Selene, TarrikRecord));
		if (!W.Handoff(C.Missions[1])) { AddError(W.Error); return false; }
		FString Error;
		TestFalse(TEXT("Tarrik's record is refused by Selene's pawn"), W.PS->RestoreProtagonistSnapshot(W.Current(), TarrikRecord, false, Error));
		ExpectSelene(*this, W, TEXT("Selene after refusing Tarrik's record"), { 0, 0, 0, 20.f });
		if (!TestTrue(TEXT("Save while Selene is active"), W.WriteSave(SeleneActiveSave))) { return false; }
	}

	FNarrativeSavePlayer TarrikOnly, SeleneActive;
	if (!ReadSave(TarrikOnlySave, TarrikOnly) || !ReadSave(SeleneActiveSave, SeleneActive)) { AddError(TEXT("Saves did not decode")); return false; }
	// A Selene-active campaign whose PlayerState predates any Selene record (legacy or damaged save).
	FNarrativeSavePlayer Damaged = SeleneActive;
	Damaged.PlayerStateData = TarrikOnly.PlayerStateData;
	{
		FPartitionWorld W; if (!W.IsValid()) { AddError(TEXT("World creation failed")); return false; }
		// The refusal is the behaviour under test: it must be reported exactly once.
		AddExpectedError(TEXT("lacks a matching protagonist snapshot"), EAutomationExpectedErrorFlags::Contains, 1);
		const ASovPlayerCharacterBase* Selene = W.Enter(C.Missions[1], &Damaged);
		if (!TestNotNull(TEXT("The Selene pawn is created for the attempted load"), Selene)) { AddError(W.Error); return false; }
		TestEqual(TEXT("Load fails explicitly"), W.PC->GetCampaignTransitionState(), ESovCampaignTransitionState::Failed);
		TestFalse(TEXT("Selene never becomes ready"), Selene->IsCharacterReady());
		TestEqual(TEXT("Selene does not borrow Tarrik's kit"), CountItems(Selene, USovPartitionTarrikKit::StaticClass()), 0);
		TestEqual(TEXT("Selene does not borrow Tarrik's currency"), Currency(Selene), 0);
		TestEqual(TEXT("Selene does not borrow Tarrik's Technique points"), W.Techniques()->GetAvailableTechniquePoints(), 0);
		FSovProtagonistSnapshot Borrowed;
		TestFalse(TEXT("Tarrik's record was not rekeyed as Selene's"), W.PS->FindProtagonistSnapshot(Tags.Character_Player_Selene, Borrowed));
	}
	return true;
}
#endif
