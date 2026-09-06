// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Components/SovCommandLinkComponent.h"
#include "Components/SovWeakPointComponent.h"

#include "Components/ActorComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Misc/AutomationTest.h"
#include "Sovereign/SovGameplayTags.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

#include <type_traits>

static_assert(std::is_base_of_v<UActorComponent, USovWeakPointComponent>);

#if WITH_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSovWeakPointExposureReplicationContractTest,
	"ProjectVelkorran.Campaign.Selene.WeakPointExposureReplicationContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSovWeakPointExposureReplicationContractTest::RunTest(
	const FString& Parameters)
{
	const USovWeakPointComponent* WeakPointCDO =
		GetDefault<USovWeakPointComponent>();
	const USovCommandLinkComponent* CommandLinkCDO =
		GetDefault<USovCommandLinkComponent>();
	TestNotNull(TEXT("Weak Point component CDO exists"), WeakPointCDO);
	TestNotNull(TEXT("Command Link component CDO exists"), CommandLinkCDO);
	if (!WeakPointCDO || !CommandLinkCDO)
	{
		return false;
	}

	TestTrue(
		TEXT("Weak Point state replication is enabled by default"),
		WeakPointCDO->GetIsReplicated());
	TestTrue(
		TEXT("A Sever reveals participant weak points by default"),
		CommandLinkCDO->IsWeakPointRevealEnabledOnSever());
	TestTrue(
		TEXT("The default Sever reveal duration is positive and finite"),
		FMath::IsFinite(CommandLinkCDO->GetWeakPointRevealDuration())
			&& CommandLinkCDO->GetWeakPointRevealDuration() > 0.0f);

	const FProperty* BrokenIdsProperty = FindFProperty<FProperty>(
		USovWeakPointComponent::StaticClass(),
		TEXT("BrokenWeakPointIds"));
	const FProperty* RevealStateProperty = FindFProperty<FProperty>(
		USovWeakPointComponent::StaticClass(),
		TEXT("WeakPointRevealState"));
	TestNotNull(TEXT("Broken Weak Point IDs are reflected"), BrokenIdsProperty);
	TestNotNull(TEXT("Timed reveal state is reflected"), RevealStateProperty);
	if (BrokenIdsProperty)
	{
		TestTrue(
			TEXT("Broken Weak Point IDs replicate"),
			BrokenIdsProperty->HasAnyPropertyFlags(CPF_Net));
		TestTrue(
			TEXT("Broken Weak Point replication rebuilds local presentation"),
			BrokenIdsProperty->HasAnyPropertyFlags(CPF_RepNotify));
		TestTrue(
			TEXT("Broken Weak Point runtime state is not serialized into assets"),
			BrokenIdsProperty->HasAnyPropertyFlags(CPF_Transient));
	}
	if (RevealStateProperty)
	{
		TestTrue(
			TEXT("Weak Point reveal timing replicates for late relevancy"),
			RevealStateProperty->HasAnyPropertyFlags(CPF_Net));
		TestTrue(
			TEXT("Replicated reveal timing rebuilds local presentation"),
			RevealStateProperty->HasAnyPropertyFlags(CPF_RepNotify));
		TestTrue(
			TEXT("Weak Point reveal runtime state is transient"),
			RevealStateProperty->HasAnyPropertyFlags(CPF_Transient));
	}

	TestNotNull(
		TEXT("Broken-zone replication has an OnRep handler"),
		USovWeakPointComponent::StaticClass()->FindFunctionByName(
			TEXT("OnRep_BrokenWeakPointIds")));
	TestNotNull(
		TEXT("Late reveal replication has an OnRep handler"),
		USovWeakPointComponent::StaticClass()->FindFunctionByName(
			TEXT("OnRep_WeakPointRevealState")));

	const auto TestAuthorityOnly = [this](const TCHAR* Description, FName Name)
	{
		const UFunction* Function =
			USovWeakPointComponent::StaticClass()->FindFunctionByName(Name);
		TestNotNull(Description, Function);
		if (Function)
		{
			TestTrue(
				*FString::Printf(TEXT("%s is authority-only"), Description),
				Function->HasAnyFunctionFlags(FUNC_BlueprintAuthorityOnly));
		}
	};
	TestAuthorityOnly(
		TEXT("Reveal Weak Points"),
		FName(TEXT("RevealWeakPoints")));
	TestAuthorityOnly(
		TEXT("Clear Weak Point Reveal"),
		FName(TEXT("ClearWeakPointReveal")));
	TestAuthorityOnly(
		TEXT("Resolve Weak Point Hit"),
		FName(TEXT("ResolveWeakPointHit")));
	TestAuthorityOnly(
		TEXT("Consume Weak Point Break"),
		FName(TEXT("ConsumeWeakPointBreak")));
	TestAuthorityOnly(
		TEXT("Reset Weak Points"),
		FName(TEXT("ResetWeakPoints")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSovWeakPointExposureZoneContractTest,
	"ProjectVelkorran.Campaign.Selene.WeakPointExposureZoneContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSovWeakPointExposureZoneContractTest::RunTest(
	const FString& Parameters)
{
	USovWeakPointComponent* WeakPoints =
		NewObject<USovWeakPointComponent>(GetTransientPackage());
	TestNotNull(TEXT("Transient Weak Point component exists"), WeakPoints);
	if (!WeakPoints)
	{
		return false;
	}

	FArrayProperty* ZonesProperty = FindFProperty<FArrayProperty>(
		USovWeakPointComponent::StaticClass(),
		TEXT("WeakPointZones"));
	FArrayProperty* BrokenIdsProperty = FindFProperty<FArrayProperty>(
		USovWeakPointComponent::StaticClass(),
		TEXT("BrokenWeakPointIds"));
	FStructProperty* RevealStateProperty = FindFProperty<FStructProperty>(
		USovWeakPointComponent::StaticClass(),
		TEXT("WeakPointRevealState"));
	FObjectPropertyBase* AbilitySystemProperty =
		FindFProperty<FObjectPropertyBase>(
			USovWeakPointComponent::StaticClass(),
			TEXT("AbilitySystemComponent"));
	TestNotNull(TEXT("Weak Point zones are reflected"), ZonesProperty);
	TestNotNull(TEXT("Broken IDs are an array"), BrokenIdsProperty);
	TestNotNull(TEXT("Reveal state is a struct"), RevealStateProperty);
	TestNotNull(TEXT("Bound Ability System is reflected"), AbilitySystemProperty);
	if (!ZonesProperty || !BrokenIdsProperty || !RevealStateProperty
		|| !AbilitySystemProperty)
	{
		return false;
	}

	TArray<FSovWeakPointZone>* Zones =
		ZonesProperty->ContainerPtrToValuePtr<TArray<FSovWeakPointZone>>(
			WeakPoints);
	TArray<FName>* BrokenIds =
		BrokenIdsProperty->ContainerPtrToValuePtr<TArray<FName>>(WeakPoints);
	FSovWeakPointRevealState* RevealState =
		RevealStateProperty->ContainerPtrToValuePtr<FSovWeakPointRevealState>(
			WeakPoints);
	TestNotNull(TEXT("Weak Point zone storage is addressable"), Zones);
	TestNotNull(TEXT("Broken ID storage is addressable"), BrokenIds);
	TestNotNull(TEXT("Reveal state storage is addressable"), RevealState);
	if (!Zones || !BrokenIds || !RevealState)
	{
		return false;
	}

	FSovWeakPointZone HornZone;
	HornZone.ZoneId = TEXT("Horn");
	HornZone.HitBones.Add(TEXT("horn_socket"));
	Zones->Add(HornZone);

	FSovWeakPointZone SocketOnlyZone;
	SocketOnlyZone.ZoneId = TEXT("PhantomSocketOnlyTarget");
	SocketOnlyZone.RevealAttachPoint = TEXT("spine_socket");
	Zones->Add(SocketOnlyZone);

	FSovWeakPointZone DuplicateHornZone = HornZone;
	DuplicateHornZone.RevealAttachPoint = TEXT("other_horn_socket");
	Zones->Add(DuplicateHornZone);

	FSovWeakPointZone DuplicatePhantomZone;
	DuplicatePhantomZone.ZoneId = SocketOnlyZone.ZoneId;
	DuplicatePhantomZone.HitBones.Add(TEXT("spine_j1"));
	Zones->Add(DuplicatePhantomZone);

	TestFalse(
		TEXT("Matcher-less and duplicate zones fail configuration validation"),
		WeakPoints->HasValidWeakPointConfiguration());

	RevealState->Serial = 1;
	RevealState->EndServerWorldTime = 30.0f;
	TestTrue(
		TEXT("A replicated future end time reconstructs an active reveal"),
		WeakPoints->IsWeakPointRevealActive());

	UNarrativeAbilitySystemComponent* AbilitySystem =
		NewObject<UNarrativeAbilitySystemComponent>(GetTransientPackage());
	TestNotNull(TEXT("Transient Narrative ASC exists"), AbilitySystem);
	if (AbilitySystem)
	{
		AbilitySystemProperty->SetObjectPropertyValue_InContainer(
			WeakPoints,
			AbilitySystem);
		AbilitySystem->AddLooseGameplayTag(
			FSovGameplayTags::Get().State_Fatal);
		TestFalse(
			TEXT("Fatal targets suppress an otherwise-live replicated reveal"),
			WeakPoints->IsWeakPointRevealActive());
		AbilitySystem->RemoveLooseGameplayTag(
			FSovGameplayTags::Get().State_Fatal);
		TestTrue(
			TEXT("Removing fatal state restores the remaining reveal window"),
			WeakPoints->IsWeakPointRevealActive());
	}
	const TArray<FName> RevealedIds = WeakPoints->GetRevealedWeakPointIds();
	TestEqual(
		TEXT("Only the first unique gameplay-matchable zone is exposed"),
		RevealedIds.Num(),
		1);
	if (RevealedIds.Num() == 1)
	{
		TestEqual(TEXT("The real Horn zone is exposed"), RevealedIds[0], FName(TEXT("Horn")));
	}

	BrokenIds->Add(TEXT("Horn"));
	TestTrue(
		TEXT("Broken zones are removed from an in-progress exposure"),
		WeakPoints->GetRevealedWeakPointIds().IsEmpty());
	BrokenIds->Reset();

	Zones->SetNum(1);
	TestTrue(
		TEXT("One unique bone-matched Horn zone is valid"),
		WeakPoints->HasValidWeakPointConfiguration());
	TestEqual(
		TEXT("The valid Horn zone remains exposed"),
		WeakPoints->GetRevealedWeakPointIds().Num(),
		1);

	RevealState->EndServerWorldTime = 0.0f;
	TestFalse(
		TEXT("A cleared replicated end time ends late-client exposure"),
		WeakPoints->IsWeakPointRevealActive());
	TestTrue(
		TEXT("Cleared exposure reveals no zones"),
		WeakPoints->GetRevealedWeakPointIds().IsEmpty());

	// This is intentionally safe without a world: presentation is cosmetic and
	// must tolerate dedicated-server/headless and teardown ordering.
	WeakPoints->RefreshWeakPointRevealPresentation();
	return true;
}

#endif
